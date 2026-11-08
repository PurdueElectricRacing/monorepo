//! Caller-managed worker. Only commands/events cross threads; telemetry stays with callers.

use std::{io, path, sync::mpsc, thread};

mod connection;
mod decode;
mod events;
mod firmware_session;
mod run;
pub mod tx;
use crate::{can, can_thread, firmware, frame, hil, superdbc, time};

#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash, serde::Serialize, serde::Deserialize)]
pub enum BusSelection {
    Vehicle,
    Charger,
    Single(u8),
}

impl Default for BusSelection {
    fn default() -> Self {
        Self::Single(0)
    }
}

impl BusSelection {
    pub const VEHICLE_BUS_NAMES: [&'static str; 3] = ["VCAN", "MCAN", "SCAN"];
    pub const CHARGER_BUS_NAMES: [&'static str; 3] = ["VCAN", "CCAN", "SCAN"];

    pub fn is_multi_bus(self) -> bool {
        matches!(self, Self::Vehicle | Self::Charger)
    }

    pub fn resolve_buses(
        self,
        database: &superdbc::database::Database,
    ) -> Option<Vec<&superdbc::database::Bus>> {
        match self {
            Self::Vehicle => Self::VEHICLE_BUS_NAMES
                .into_iter()
                .map(|name| database.bus(name))
                .collect(),
            Self::Charger => Self::CHARGER_BUS_NAMES
                .into_iter()
                .map(|name| database.bus(name))
                .collect(),
            Self::Single(bus_id) => database.bus_by_id(bus_id).map(|bus| vec![bus]),
        }
    }

    pub fn resolve_bus_ids(self, database: &superdbc::database::Database) -> Option<Vec<u8>> {
        self.resolve_buses(database)
            .map(|buses| buses.into_iter().map(|bus| bus.bus_id()).collect())
    }

    pub fn fallback_bus_ids(self) -> Vec<u8> {
        match self {
            Self::Vehicle => vec![0, 1, 3],
            Self::Charger => vec![0, 2, 3],
            Self::Single(bus_id) => vec![bus_id],
        }
    }

    pub fn display_name(self, database: &superdbc::database::Database) -> String {
        match self {
            Self::Vehicle => "Vehicle (VCAN, MCAN, SCAN)".to_owned(),
            Self::Charger => "Charger (VCAN, CCAN, SCAN)".to_owned(),
            Self::Single(bus_id) => match database.bus_by_id(bus_id) {
                Some(bus) => format!("{} ({})", bus.name(), bus_id),
                None => format!("Bus {bus_id}"),
            },
        }
    }
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct DatabaseSource {
    pub path: path::PathBuf,
    pub selection: BusSelection,
}

/// A file replacement loads on the worker.
/// A bus change reuses its active database.
#[derive(Debug, Clone)]
pub enum DatabaseRequest {
    Load {
        path: path::PathBuf,
        preferred_selection: Option<BusSelection>,
    },
    Bus {
        selection: BusSelection,
    },
}

/// Successful selection, sent only after the worker applies the new context.
#[derive(Debug)]
pub enum DatabaseUpdate {
    Loaded {
        database: Box<superdbc::database::Database>,
        selection: BusSelection,
    },
    Bus {
        selection: BusSelection,
    },
}

#[derive(Default)]
pub struct CanThreadConfig {
    pub log_folder: Option<path::PathBuf>,
    pub hil_dir: path::PathBuf,
}

pub enum CanThreadCommand {
    Connect(Option<crate::connection::ConnectionSource>),
    SelectDatabase {
        request_id: u64,
        selection: DatabaseRequest,
    },
    AddSendMessage(can_thread::tx::AddSendMessage),
    DeleteSendMessage {
        key: superdbc::message::MessageKey,
    },
    UpdateLogFolder(path::PathBuf),
    Stop,
    Hil(hil::engine::HilCommand),
    StartFirmwareUpdate(firmware::protocol::FirmwarePackage),
    ArmFirmwareUpdate(firmware::protocol::FirmwarePackage),
    CancelFirmwareUpdate,
}

pub enum CanThreadEvent {
    DatabaseSelected {
        request_id: u64,
        result: Result<DatabaseUpdate, String>,
    },
    Frame(frame::ParsedFrame),
    SourceSelected(Option<crate::connection::ConnectionSource>),
    Disconnection,
    ConnectionSuccessful,
    ConnectionFailed(String),
    MessageSent {
        key: superdbc::message::MessageKey,
        timestamp: time::Time,
        amount_left: Option<can_thread::tx::SendAmount>,
    },
    SendFailed {
        key: superdbc::message::MessageKey,
        error: String,
        retrying: bool,
    },
    Diagnostic(String),
    BusLoad {
        timestamp: time::Time,
        buses: Vec<can::bus_load::BusLoadReading>,
    },
    Hil(hil::engine::HilSnapshot),
    FirmwareProgress(firmware::FirmwareProgress),
}

/// Single caller owns shutdown; cloned senders submit commands but do not own the worker.
pub struct CanThreadHandle {
    tx: mpsc::Sender<CanThreadCommand>,
    join: Option<thread::JoinHandle<()>>,
}

impl CanThreadHandle {
    pub fn command(
        &self,
        command: CanThreadCommand,
    ) -> Result<(), mpsc::SendError<CanThreadCommand>> {
        self.tx.send(command)
    }

    pub fn sender(&self) -> mpsc::Sender<CanThreadCommand> {
        self.tx.clone()
    }

    pub fn stop(&mut self) -> thread::Result<()> {
        let _ = self.tx.send(CanThreadCommand::Stop);
        match self.join.take() {
            Some(thread) => thread.join(),
            None => Ok(()),
        }
    }
}

impl Drop for CanThreadHandle {
    fn drop(&mut self) {
        let _ = self.stop();
    }
}

pub fn spawn_can_thread(
    config: CanThreadConfig,
    out: mpsc::Sender<CanThreadEvent>,
) -> io::Result<CanThreadHandle> {
    let (tx, rx) = mpsc::channel();
    let join = thread::Builder::new()
        .name("daq-can".into())
        .spawn(move || run::run(config, out, rx))?;
    Ok(CanThreadHandle {
        tx,
        join: Some(join),
    })
}
