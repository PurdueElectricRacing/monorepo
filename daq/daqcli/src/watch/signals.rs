//! Named physical-value injection through the shared SuperDBC encoder.

use daqcore::{frame, superdbc};

/// Parses `signals MESSAGE SIGNAL=VALUE ...` on the selected bus.
pub fn parse(
    line: &str,
    database: &superdbc::database::Database,
    bus_id: u8,
) -> Result<frame::CanFrameTx, String> {
    let mut fields = line.split_whitespace().skip(1);
    let name = fields
        .next()
        .ok_or_else(|| "expected signals MESSAGE SIGNAL=VALUE ...".to_owned())?;
    let bus = database
        .bus_by_id(bus_id)
        .ok_or_else(|| format!("SuperDBC has no bus {bus_id}"))?;
    let message = bus
        .messages()
        .iter()
        .find(|message| message.name() == name)
        .ok_or_else(|| format!("bus {} has no message {name}", bus.name()))?;

    // Preserve duplicates so the shared encoder rejects them along with unknown
    // and missing signals. Never fill omitted values from defaults or RX state.
    let values = fields
        .map(parse_assignment)
        .collect::<Result<Vec<_>, _>>()?;
    let data = message.encode(&values).map_err(|error| error.to_string())?;
    frame::CanFrameTx::data(message.identity(), message.is_extended_id(), data)
}

fn parse_assignment(text: &str) -> Result<(&str, f64), String> {
    let (name, value) = text
        .split_once('=')
        .ok_or_else(|| format!("expected SIGNAL=VALUE, got {text:?}"))?;
    let value = value
        .parse::<f64>()
        .map_err(|_| format!("signal {name:?} requires a physical numeric value"))?;
    Ok((name, value))
}
