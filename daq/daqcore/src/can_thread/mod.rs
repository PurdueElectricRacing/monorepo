//! Caller-managed worker. Only commands/events cross threads; telemetry stays with callers.

mod connection;
mod decode;
mod events;
mod firmware_session;
mod run;
mod tx;
use crate::{ParsedFrame, Time, connection::ConnectionSource, firmware, hil};
use std::{path::PathBuf, sync::mpsc, thread::JoinHandle};
pub use tx::{AddSendMessage, SendAmount};
#[derive(Default)]
pub struct CanThreadConfig {
    pub dbc_path: Option<PathBuf>,
    pub log_folder: Option<PathBuf>,
    pub hil_dir: PathBuf,
}
pub enum CanThreadCommand {
    Connect(Option<ConnectionSource>),
    DbcSelected(PathBuf),
    AddSendMessage(AddSendMessage),
    DeleteSendMessage { msg_id: u32 },
    UpdateLogFolder(PathBuf),
    Stop,
    Hil(hil::engine::HilCommand),
    StartFirmwareUpdate(firmware::FirmwarePackage),
    ArmFirmwareUpdate(firmware::FirmwarePackage),
    CancelFirmwareUpdate,
}
pub enum CanThreadEvent {
    Frame(ParsedFrame),
    SourceSelected(Option<ConnectionSource>),
    Disconnection,
    ConnectionSuccessful,
    ConnectionFailed(String),
    MessageSent {
        msg_id: u32,
        timestamp: Time,
        amount_left: Option<SendAmount>,
    },
    SendFailed {
        msg_id: u32,
        error: String,
        retrying: bool,
    },
    Diagnostic(String),
    BusLoad {
        timestamp: Time,
        load_1s: f32,
        load_5s: f32,
        load_10s: f32,
        load_30s: f32,
    },
    Hil(hil::engine::HilSnapshot),
    FirmwareProgress(firmware::FirmwareProgress),
}
/// Single caller owns shutdown; cloned senders submit commands but do not own the worker.
pub struct CanThreadHandle {
    tx: mpsc::Sender<CanThreadCommand>,
    join: Option<JoinHandle<()>>,
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
    pub fn stop(&mut self) -> std::thread::Result<()> {
        let _ = self.tx.send(CanThreadCommand::Stop);
        self.join.take().map_or(Ok(()), |j| j.join())
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
) -> std::io::Result<CanThreadHandle> {
    let (tx, rx) = mpsc::channel();
    let join = std::thread::Builder::new()
        .name("daq-can".into())
        .spawn(move || run::run(config, out, rx))?;
    Ok(CanThreadHandle {
        tx,
        join: Some(join),
    })
}
