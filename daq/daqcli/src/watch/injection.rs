//! Stdin CAN injection commands.

use daqcore::{can, can_thread, frame, superdbc};
use std::{
    io::{self, BufRead as _},
    sync::{
        Arc,
        atomic::{AtomicBool, Ordering},
        mpsc,
    },
};

/// Decoding context for stdin injection lines.
pub enum InjectionContext {
    /// No SuperDBC artifact; `ID:HEXDATA` lines only.
    Raw,
    /// SuperDBC-selected bus; `BUS[@TIME_MS]:ID:HEXDATA` lines allowed.
    Defined {
        database: Box<superdbc::database::Database>,
        bus_id: u8,
        bus_name: String,
    },
}

/// Reads CAN injection commands from stdin for as long as it is open.
pub struct StdinInjections {
    context: InjectionContext,
    sender: mpsc::Sender<can_thread::CanThreadCommand>,
    quit_requested: Arc<AtomicBool>,
}

impl StdinInjections {
    pub fn new(
        context: InjectionContext,
        sender: mpsc::Sender<can_thread::CanThreadCommand>,
        quit_requested: Arc<AtomicBool>,
    ) -> Self {
        Self {
            context,
            sender,
            quit_requested,
        }
    }

    pub fn run(self) {
        for line in io::stdin().lock().lines() {
            let line = match line {
                Ok(line) => line,
                Err(error) => {
                    log::error!("failed to read stdin: {error}");
                    return;
                }
            };
            let line = line.trim();
            if line.is_empty() {
                continue;
            }
            if line == "quit" || line == "exit" {
                self.quit_requested.store(true, Ordering::SeqCst);
                // A closed worker is reported by the monitor, not from here.
                let _ = self.sender.send(can_thread::CanThreadCommand::Stop);
                return;
            }
            match parse_injection(line, &self.context) {
                Ok(command) => {
                    if self.sender.send(command).is_err() {
                        // The worker already stopped; the monitor reports the shutdown.
                        return;
                    }
                }
                Err(error) => log::warn!("ignored stdin CAN command: {error}"),
            }
        }
        log::info!("stdin closed; continuing without injection input");
    }
}

/// Parses a stdin injection line, validating against daqcore frame rules.
fn parse_injection(
    line: &str,
    context: &InjectionContext,
) -> Result<can_thread::CanThreadCommand, String> {
    let mut fields = line.splitn(3, ':');
    let first = fields.next().ok_or_else(|| injection_format(context))?;
    let second = fields.next().ok_or_else(|| injection_format(context))?;
    let third = fields.next();

    let (id_text, data_text) = match third {
        None => (first, second),
        Some(data_text) => {
            match context {
                InjectionContext::Defined {
                    bus_id, bus_name, ..
                } => validate_injection_bus(first, *bus_id, bus_name)?,
                InjectionContext::Raw => {
                    return Err("bus prefixes require --superdbc; use ID:HEXDATA".to_owned());
                }
            }
            (second, data_text)
        }
    };

    let identity = parse_unsigned(id_text)
        .filter(|identity| *identity <= u64::from(can::EXTENDED_ID_MASK))
        .ok_or_else(|| "invalid CAN injection identifier".to_owned())?;
    let identity =
        u32::try_from(identity).map_err(|_| "invalid CAN injection identifier".to_owned())?;

    let is_extended_id = match context {
        InjectionContext::Defined {
            database, bus_id, ..
        } => database.message(*bus_id, identity).map_or(
            identity > can::STANDARD_ID_MASK,
            superdbc::message::Message::is_extended_id,
        ),
        InjectionContext::Raw => identity > can::STANDARD_ID_MASK,
    };

    let data = parse_hex_bytes(data_text)?;
    let frame = frame::CanFrameTx::data(identity, is_extended_id, data)?;
    Ok(can_thread::CanThreadCommand::SendOnce(frame))
}

fn injection_format(context: &InjectionContext) -> String {
    match context {
        InjectionContext::Raw => "expected ID:HEXDATA".to_owned(),
        InjectionContext::Defined { .. } => {
            "expected BUS[@TIME_MS]:ID:HEXDATA or ID:HEXDATA".to_owned()
        }
    }
}

/// Validates a `BUS[@TIME_MS]` prefix against the selected SuperDBC bus.
fn validate_injection_bus(endpoint: &str, bus_id: u8, bus_name: &str) -> Result<(), String> {
    let (bus, at_time) = match endpoint.split_once('@') {
        Some((bus, at_time)) => (bus, Some(at_time)),
        None => (endpoint, None),
    };
    if let Some(at_time) = at_time {
        if at_time.is_empty() || parse_unsigned(at_time).is_none() {
            return Err("invalid CAN injection time".to_owned());
        }
        // `fil watch` parses a time prefix but injects stdin frames immediately too.
        log::debug!("ignoring stdin injection time; sending immediately");
    }
    let numeric = bus.parse::<u8>().ok();
    if bus != bus_name && numeric != Some(bus_id) {
        return Err(format!(
            "injection names bus {bus}, but {bus_name} is selected"
        ));
    }
    Ok(())
}

fn parse_unsigned(text: &str) -> Option<u64> {
    match text.strip_prefix("0x").or_else(|| text.strip_prefix("0X")) {
        Some(digits) => u64::from_str_radix(digits, 16).ok(),
        None => text.parse::<u64>().ok(),
    }
}

fn parse_hex_bytes(text: &str) -> Result<Vec<u8>, String> {
    if !text.len().is_multiple_of(2) {
        return Err("CAN injection payload must contain complete hex bytes".to_owned());
    }
    text.as_bytes()
        .as_chunks::<2>()
        .0
        .iter()
        .map(|pair| {
            let high = hex_nibble(pair[0])
                .ok_or_else(|| "CAN injection payload is not hexadecimal".to_owned())?;
            let low = hex_nibble(pair[1])
                .ok_or_else(|| "CAN injection payload is not hexadecimal".to_owned())?;
            Ok((high << 4) | low)
        })
        .collect()
}

fn hex_nibble(byte: u8) -> Option<u8> {
    match byte {
        b'0'..=b'9' => Some(byte - b'0'),
        b'a'..=b'f' => Some(byte - b'a' + 10),
        b'A'..=b'F' => Some(byte - b'A' + 10),
        _ => None,
    }
}
