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
        let ids: Vec<u8> = self
            .active_buses()
            .into_iter()
            .map(|bus| bus.bus_id())
            .collect();
        if ids.is_empty() { vec![0] } else { ids }
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
            None => active_ids.first().copied(),
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
