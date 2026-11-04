//! Frontend-owned selection and connection state shared by DaqApp and DaqCLI.

use crate::{can_thread, connection, superdbc};
use std::sync::mpsc;

pub struct DatabaseSelection {
    pub database: superdbc::database::Database,
    pub bus_id: u8,
}

impl DatabaseSelection {
    pub fn bus(&self) -> Option<&superdbc::database::Bus> {
        self.database.bus_by_id(self.bus_id)
    }

    pub fn source(&self) -> Option<can_thread::DatabaseSource> {
        Some(can_thread::DatabaseSource {
            path: self.database.path()?.to_owned(),
            bus_id: self.bus_id,
        })
    }
}

#[derive(Debug, Default, PartialEq, Clone)]
pub enum ConnectionStatus {
    #[default]
    Disconnected,
    Connected,
    Error(String),
}

#[derive(Default)]
pub struct CanState {
    pub database: Option<DatabaseSelection>,
    pub connection_status: ConnectionStatus,
    pub active_source: Option<connection::ConnectionSource>,
    pending_database: Option<u64>,
    selection_request: u64,
}

impl CanState {
    pub fn selection_pending(&self) -> bool {
        self.pending_database.is_some()
    }

    pub fn request_selection(
        &mut self,
        sender: &mpsc::Sender<can_thread::CanThreadCommand>,
        selection: can_thread::DatabaseRequest,
    ) -> Result<(), String> {
        if self.selection_pending() {
            return Ok(());
        }
        if let can_thread::DatabaseRequest::Bus { bus_id } = &selection {
            let Some(current) = &self.database else {
                return Err("No database is loaded".into());
            };
            if current.bus_id == *bus_id {
                return Ok(());
            }
            if current.database.bus_by_id(*bus_id).is_none() {
                return Err(format!("SuperDBC has no bus {bus_id}"));
            }
        }
        self.selection_request += 1;
        let request_id = self.selection_request;
        sender
            .send(can_thread::CanThreadCommand::SelectDatabase {
                request_id,
                selection,
            })
            .map_err(|error| error.to_string())?;
        self.pending_database = Some(request_id);
        Ok(())
    }

    pub fn connect(
        &mut self,
        sender: &mpsc::Sender<can_thread::CanThreadCommand>,
        source: connection::ConnectionSource,
    ) -> Result<(), String> {
        sender
            .send(can_thread::CanThreadCommand::Connect(Some(source)))
            .map_err(|error| error.to_string())?;
        self.connection_status = ConnectionStatus::Disconnected;
        Ok(())
    }

    /// Returns false for stale responses; callers reset presentation only on a change.
    pub fn apply_database_result(
        &mut self,
        request_id: u64,
        result: Result<can_thread::DatabaseUpdate, String>,
    ) -> Result<bool, String> {
        if self.pending_database != Some(request_id) {
            return Ok(false);
        }
        self.pending_database = None;
        match result? {
            can_thread::DatabaseUpdate::Loaded { database, bus_id } => {
                if database.bus_by_id(bus_id).is_none() {
                    return Err(format!("SuperDBC has no bus {bus_id}"));
                }
                self.database = Some(DatabaseSelection {
                    database: *database,
                    bus_id,
                });
            }
            can_thread::DatabaseUpdate::Bus { bus_id } => {
                let selection = self.database.as_mut().ok_or("No database is loaded")?;
                if selection.database.bus_by_id(bus_id).is_none() {
                    return Err(format!("SuperDBC has no bus {bus_id}"));
                }
                selection.bus_id = bus_id;
            }
        }
        Ok(true)
    }

    /// Returns whether the active source changed, including deselection.
    pub fn observe_connection(&mut self, event: &can_thread::CanThreadEvent) -> bool {
        match event {
            can_thread::CanThreadEvent::SourceSelected(source) => {
                let changed = self.active_source != *source;
                self.active_source = source.clone();
                return changed;
            }
            can_thread::CanThreadEvent::ConnectionSuccessful => {
                self.connection_status = ConnectionStatus::Connected
            }
            can_thread::CanThreadEvent::ConnectionFailed(error) => {
                self.connection_status = ConnectionStatus::Error(error.clone())
            }
            can_thread::CanThreadEvent::Disconnection => {
                self.connection_status = ConnectionStatus::Disconnected
            }
            _ => {}
        }
        false
    }
}
