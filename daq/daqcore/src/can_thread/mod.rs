//! Caller-managed worker. Only commands/events cross threads; telemetry stays with callers.

use std::{io, path, sync::mpsc, thread};

mod connection;
mod decode;
mod events;
mod firmware_session;
mod run;
pub mod tx;
use crate::{can_thread, firmware, frame, hil, superdbc, time};

pub use crate::can::driver::FilGpioDirection;

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct DatabaseSource {
    pub path: path::PathBuf,
    pub bus_id: u8,
}

/// A file replacement loads on the worker.
/// A bus change reuses its active database.
#[derive(Debug, Clone)]
pub enum DatabaseRequest {
    Load {
        path: path::PathBuf,
        preferred_bus: Option<u8>,
    },
    Bus {
        bus_id: u8,
    },
}

/// Successful selection, sent only after the worker applies the new context.
#[derive(Debug)]
pub enum DatabaseUpdate {
    Loaded {
        database: Box<superdbc::database::Database>,
        bus_id: u8,
    },
    Bus {
        bus_id: u8,
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
        identity: frame::CanIdentity,
    },
    UpdateLogFolder(path::PathBuf),
    Stop,
    Hil(hil::engine::HilCommand),
    StartFirmwareUpdate(firmware::protocol::FirmwarePackage),
    ArmFirmwareUpdate(firmware::protocol::FirmwarePackage),
    CancelFirmwareUpdate,
    SetFilAdc {
        board: String,
        instance: String,
        channel: u8,
        value: u16,
    },
    SetFilGpio {
        board: String,
        port: String,
        pin: u8,
        value: Option<bool>,
    },
    SetFilTraceBus(Option<String>),
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
        identity: frame::CanIdentity,
        timestamp: time::Time,
        amount_left: Option<can_thread::tx::SendAmount>,
    },
    SendFailed {
        identity: frame::CanIdentity,
        error: String,
        retrying: bool,
    },
    Diagnostic(String),
    BusLoad {
        timestamp: time::Time,
        load_1s: f32,
        load_5s: f32,
        load_10s: f32,
        load_30s: f32,
    },
    Hil(hil::engine::HilSnapshot),
    FirmwareProgress(firmware::FirmwareProgress),
    FilGpio {
        board: String,
        port: String,
        pin: u8,
        value: Option<bool>,
        direction: crate::can::driver::FilGpioDirection,
    },
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
