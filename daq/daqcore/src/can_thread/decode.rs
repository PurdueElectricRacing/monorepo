use crate::{frame, superdbc, time};

pub fn bus(
    database: Option<&superdbc::database::Database>,
    bus_id: u8,
) -> Option<&superdbc::database::Bus> {
    database?.bus_by_id(bus_id)
}

pub fn decode(
    frame: frame::CanFrame,
    timestamp: time::Time,
    database: Option<&superdbc::database::Database>,
    bus_id: u8,
) -> frame::ParsedFrame {
    let decoded = if frame.kind == frame::FrameKind::Data {
        bus(database, bus_id).and_then(|bus| bus.decode(frame.identity, &frame.data).ok())
    } else {
        None
    };
    frame::ParsedFrame {
        kind: frame.kind,
        dlc: frame.dlc,
        timestamp,
        identity: frame.identity,
        raw_bytes: frame.data,
        decoded,
    }
}
