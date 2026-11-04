//! Stdin CAN injection commands.

use crate::watch;
use daqcore::{can, can_thread, frame, superdbc};
use std::{
    io::{self, BufRead as _},
    sync::mpsc,
};

/// Decoding context for stdin injection lines.
pub enum InjectionContext {
    /// No SuperDBC artifact; `ID:HEXDATA` lines only.
    Raw,
    /// SuperDBC-selected bus; bus prefixes and named signal commands allowed.
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
    quit_requested: mpsc::Sender<()>,
}

impl StdinInjections {
    pub fn new(
        context: InjectionContext,
        sender: mpsc::Sender<can_thread::CanThreadCommand>,
        quit_requested: mpsc::Sender<()>,
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
                // Notify the monitor before the worker closes its event channel.
                if self.quit_requested.send(()).is_err() {
                    return;
                }
                // A closed worker is reported by the monitor, not from here.
                let _ = self.sender.send(can_thread::CanThreadCommand::DrainAndStop);
                return;
            }
            let frame = match parse_injection(line, &self.context) {
                Ok(frame) => frame,
                Err(error) => {
                    log::warn!("ignored stdin CAN command: {error}");
                    continue;
                }
            };
            if !self.submit_frame(frame) {
                return;
            }
        }
        log::info!("stdin closed; continuing without injection input");
    }

    fn submit_frame(&self, frame: frame::CanFrameTx) -> bool {
        // One outstanding submission bounds the shared command backlog
        // without making GUI command senders block on a full channel.
        let (processed_tx, processed_rx) = mpsc::sync_channel(1);
        let command = can_thread::CanThreadCommand::InjectFrame {
            frame,
            processed: processed_tx,
        };
        // A stopped worker is reported by the monitor, not the stdin reader.
        self.sender.send(command).is_ok() && processed_rx.recv().is_ok()
    }
}

/// Parses a stdin injection line, validating against daqcore frame rules.
fn parse_injection(line: &str, context: &InjectionContext) -> Result<frame::CanFrameTx, String> {
    if line.split_whitespace().next() == Some("signals") {
        return match context {
            InjectionContext::Raw => Err("signal commands require --superdbc".to_owned()),
            InjectionContext::Defined {
                database, bus_id, ..
            } => watch::signals::parse(line, database, *bus_id),
        };
    }
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

    let identity = parse_identity(id_text)?;

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
    frame::CanFrameTx::data(identity, is_extended_id, data)
}

fn injection_format(context: &InjectionContext) -> String {
    match context {
        InjectionContext::Raw => "expected ID:HEXDATA".to_owned(),
        InjectionContext::Defined { .. } => "expected BUS:ID:HEXDATA or ID:HEXDATA".to_owned(),
    }
}

/// Validates a `BUS` prefix against the selected SuperDBC bus.
fn validate_injection_bus(endpoint: &str, bus_id: u8, bus_name: &str) -> Result<(), String> {
    let bus = endpoint;
    let numeric = bus.parse::<u8>().ok();
    if bus != bus_name && numeric != Some(bus_id) {
        return Err(format!(
            "injection names bus {bus}, but {bus_name} is selected"
        ));
    }
    Ok(())
}

fn parse_identity(text: &str) -> Result<frame::CanIdentity, String> {
    let parsed = match text.strip_prefix("0x").or_else(|| text.strip_prefix("0X")) {
        Some(digits) => u32::from_str_radix(digits, 16),
        None => text.parse::<u32>(),
    };
    let identity = parsed.map_err(|_| "invalid CAN injection identifier".to_owned())?;
    if identity > can::EXTENDED_ID_MASK {
        return Err("CAN injection identifier exceeds 29 bits".to_owned());
    }
    Ok(identity)
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
