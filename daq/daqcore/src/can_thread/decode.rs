use crate::{can_thread, frame, superdbc, time};

pub fn active_buses(
    database: Option<&superdbc::database::Database>,
    selection: can_thread::BusSelection,
) -> Vec<&superdbc::database::Bus> {
    database
        .as_ref()
        .and_then(|db| selection.resolve_buses(db))
        .unwrap_or_default()
}

/// Return the frame's bus ID, falling back to the primary active bus (or 0)
/// when the driver does not report one.
pub fn raw_frame_bus(
    database: Option<&superdbc::database::Database>,
    selection: can_thread::BusSelection,
    frame: &frame::CanFrame,
) -> u8 {
    frame
        .bus_id
        .or_else(|| {
            active_buses(database, selection)
                .first()
                .map(|bus| bus.bus_id())
        })
        .unwrap_or(0)
}

/// Resolve the bus ID for a received frame, returning `None` if the frame
/// belongs to a bus outside the active selection.
pub fn resolve_frame_bus(
    database: Option<&superdbc::database::Database>,
    selection: can_thread::BusSelection,
    frame: &frame::CanFrame,
) -> Option<u8> {
    let bus_id = raw_frame_bus(database, selection, frame);
    let buses = active_buses(database, selection);
    (buses.is_empty() || buses.iter().any(|bus| bus.bus_id() == bus_id)).then_some(bus_id)
}

pub fn decode(
    database: Option<&superdbc::database::Database>,
    frame: frame::CanFrame,
    bus_id: u8,
    timestamp: time::Time,
) -> frame::ParsedFrame {
    let decoded = if frame.kind == frame::FrameKind::Data {
        database
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
