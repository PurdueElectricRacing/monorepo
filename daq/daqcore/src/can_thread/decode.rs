use crate::{can_thread, frame, superdbc, time};

#[derive(Default)]
pub struct FrameDecoder {
    database: Option<superdbc::database::Database>,
    selection: can_thread::BusSelection,
}

impl FrameDecoder {
    /// Load and validate file replacements on the worker
    /// Failures leave both the definitions and bus selection unchanged.
    pub fn select(
        &mut self,
        request: &can_thread::DatabaseRequest,
    ) -> Result<can_thread::DatabaseUpdate, String> {
        match request {
            can_thread::DatabaseRequest::Load {
                path,
                preferred_selection,
            } => {
                let database =
                    superdbc::database::Database::load(path).map_err(|error| error.to_string())?;
                let selection = preferred_selection
                    .filter(|sel| sel.resolve_buses(&database).is_some())
                    .or_else(|| {
                        can_thread::BusSelection::Vehicle
                            .resolve_buses(&database)
                            .map(|_| can_thread::BusSelection::Vehicle)
                    })
                    .or_else(|| {
                        database
                            .bus_by_id(0)
                            .map(|bus| can_thread::BusSelection::Single(bus.bus_id()))
                    })
                    .or_else(|| {
                        database
                            .buses()
                            .first()
                            .map(|bus| can_thread::BusSelection::Single(bus.bus_id()))
                    })
                    .ok_or_else(|| "SuperDBC contains no selectable buses".to_owned())?;
                let update = can_thread::DatabaseUpdate::Loaded {
                    database: Box::new(database.clone()),
                    selection,
                };
                self.database = Some(database);
                self.selection = selection;
                Ok(update)
            }
            can_thread::DatabaseRequest::Bus { selection } => {
                let database = self
                    .database
                    .as_ref()
                    .ok_or_else(|| "No SuperDBC database is loaded".to_owned())?;
                if selection.resolve_buses(database).is_none() {
                    return Err(format!("SuperDBC does not support {:?}", selection));
                }
                self.selection = *selection;
                Ok(can_thread::DatabaseUpdate::Bus {
                    selection: *selection,
                })
            }
        }
    }

    pub fn active_buses(&self) -> Vec<&superdbc::database::Bus> {
        self.database
            .as_ref()
            .and_then(|db| self.selection.resolve_buses(db))
            .unwrap_or_default()
    }

    pub fn active_bus_ids(&self) -> Vec<u8> {
        self.database
            .as_ref()
            .and_then(|db| self.selection.resolve_bus_ids(db))
            .unwrap_or_else(|| self.selection.fallback_bus_ids())
    }

    pub fn bus_by_id(&self, bus_id: u8) -> Option<&superdbc::database::Bus> {
        self.database.as_ref()?.bus_by_id(bus_id)
    }

    /// Resolve the bus ID for a received frame, returning `None` if the frame
    /// belongs to a bus outside the active selection.
    pub fn resolve_frame_bus(&self, frame: &frame::CanFrame) -> Option<u8> {
        let active_ids = self.active_bus_ids();
        match frame.bus_id {
            Some(bus_id) => active_ids.contains(&bus_id).then_some(bus_id),
            None => match self.selection {
                can_thread::BusSelection::Single(bus_id) => Some(bus_id),
                _ => active_ids.first().copied(),
            },
        }
    }

    pub fn decode(
        &self,
        frame: frame::CanFrame,
        bus_id: u8,
        timestamp: time::Time,
    ) -> frame::ParsedFrame {
        let decoded = if frame.kind == frame::FrameKind::Data {
            self.bus_by_id(bus_id)
                .and_then(|bus| bus.decode(frame.identity, &frame.data).ok())
        } else {
            None
        };
        frame::ParsedFrame {
            kind: frame.kind,
            dlc: frame.dlc,
            timestamp,
            bus_id,
            identity: frame.identity,
            raw_bytes: frame.data,
            decoded,
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::path::PathBuf;

    fn sample_superdbc_path() -> PathBuf {
        PathBuf::from(env!("CARGO_MANIFEST_DIR"))
            .parent()
            .unwrap()
            .parent()
            .unwrap()
            .join("superdbc/superdbc_68f90f7.json")
    }

    #[test]
    fn frame_decoder_resolves_presets_and_decodes_colliding_ids_across_buses() {
        let mut decoder = FrameDecoder::default();
        let path = sample_superdbc_path();
        let update = decoder
            .select(&can_thread::DatabaseRequest::Load {
                path,
                preferred_selection: Some(can_thread::BusSelection::Vehicle),
            })
            .expect("should load SuperDBC");
        assert!(matches!(
            update,
            can_thread::DatabaseUpdate::Loaded {
                selection: can_thread::BusSelection::Vehicle,
                ..
            }
        ));
        assert_eq!(decoder.active_bus_ids(), vec![0, 1, 3]);

        // Frame on CCAN (bus 2) should be filtered out in Vehicle mode
        let ccan_frame =
            frame::CanFrame::data_on_bus(Some(2), 0x03C, vec![0; 8]).expect("valid frame");
        assert_eq!(decoder.resolve_frame_bus(&ccan_frame), None);

        // Colliding ID 0x03C exists on both VCAN (0) and SCAN (3)
        let vcan_frame =
            frame::CanFrame::data_on_bus(Some(0), 0x03C, vec![0; 8]).expect("valid frame");
        let scan_frame =
            frame::CanFrame::data_on_bus(Some(3), 0x03C, vec![0; 8]).expect("valid frame");

        assert_eq!(decoder.resolve_frame_bus(&vcan_frame), Some(0));
        assert_eq!(decoder.resolve_frame_bus(&scan_frame), Some(3));

        let parsed_vcan = decoder.decode(vcan_frame, 0, time::Time::now());
        let parsed_scan = decoder.decode(scan_frame, 3, time::Time::now());

        assert_eq!(parsed_vcan.bus_id, 0);
        assert_eq!(parsed_scan.bus_id, 3);
        assert!(parsed_vcan.decoded.is_some());
        assert!(parsed_scan.decoded.is_some());
        assert_eq!(
            parsed_vcan.decoded.as_ref().unwrap().key(),
            superdbc::message::MessageKey {
                bus_id: 0,
                identity: 0x03C
            }
        );
        assert_eq!(
            parsed_scan.decoded.as_ref().unwrap().key(),
            superdbc::message::MessageKey {
                bus_id: 3,
                identity: 0x03C
            }
        );

        // Switch to Charger preset (VCAN=0, CCAN=2, SCAN=3)
        decoder
            .select(&can_thread::DatabaseRequest::Bus {
                selection: can_thread::BusSelection::Charger,
            })
            .expect("should switch to Charger");
        assert_eq!(decoder.active_bus_ids(), vec![0, 2, 3]);
        assert_eq!(decoder.resolve_frame_bus(&ccan_frame), Some(2));
        let mcan_frame =
            frame::CanFrame::data_on_bus(Some(1), 0x100, vec![0; 8]).expect("valid frame");
        assert_eq!(decoder.resolve_frame_bus(&mcan_frame), None);
    }
}
