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
                let selection = [
                    *preferred_selection,
                    Some(can_thread::BusSelection::Vehicle),
                ]
                .into_iter()
                .flatten()
                .find(|sel| sel.resolve_buses(&database).is_some())
                .or_else(|| can_thread::BusSelection::default_single(&database))
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

    /// Return the frame's bus ID, falling back to the primary active bus (or 0)
    /// when the driver does not report one.
    pub fn raw_frame_bus(&self, frame: &frame::CanFrame) -> u8 {
        frame
            .bus_id
            .or_else(|| self.active_buses().first().map(|bus| bus.bus_id()))
            .unwrap_or(0)
    }

    /// Resolve the bus ID for a received frame, returning `None` if the frame
    /// belongs to a bus outside the active selection.
    pub fn resolve_frame_bus(&self, frame: &frame::CanFrame) -> Option<u8> {
        let bus_id = self.raw_frame_bus(frame);
        let buses = self.active_buses();
        (buses.is_empty() || buses.iter().any(|bus| bus.bus_id() == bus_id)).then_some(bus_id)
    }

    pub fn decode(
        &self,
        frame: frame::CanFrame,
        bus_id: u8,
        timestamp: time::Time,
    ) -> frame::ParsedFrame {
        let decoded = if frame.kind == frame::FrameKind::Data {
            self.database
                .as_ref()
                .and_then(|db| db.bus_by_id(bus_id))
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
