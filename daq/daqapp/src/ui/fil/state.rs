use super::*;

impl FilControl {
    pub fn refresh_network_info(&mut self) {
        if self.last_loaded_network == self.settings.network {
            return;
        }
        self.last_loaded_network = self.settings.network.clone();
        self.network_info = None;
        self.network_info_error = None;
        let Some(network) = self.settings.network.clone() else {
            return;
        };
        let (tx, rx) = mpsc::channel();
        self.network_info_rx = Some((network.clone(), rx));
        thread::spawn(move || {
            let _ = tx.send(config::load_network_info(&network));
        });
    }
    pub fn poll_network_info(&mut self, actions: &mut Vec<action::AppAction>) {
        let Some((path, receiver)) = &self.network_info_rx else {
            return;
        };
        if !network_result_is_current(self.settings.network.as_deref(), path) {
            self.network_info_rx = None;
            return;
        }
        match receiver.try_recv() {
            Ok(result) => {
                self.network_info_rx = None;
                match result {
                    Ok(info) => {
                        let boards: Vec<String> =
                            info.boards.iter().map(|board| board.name.clone()).collect();
                        if !boards.contains(&self.settings.adc_board) && !boards.is_empty() {
                            self.settings.adc_board = boards[0].clone();
                        }
                        if !boards.contains(&self.gpio_board) && !boards.is_empty() {
                            self.gpio_board = boards[0].clone();
                        }
                        self.network_info = Some(info);
                        self.queue_config_update(actions);
                    }
                    Err(error) => self.network_info_error = Some(error),
                }
            }
            Err(mpsc::TryRecvError::Empty) => {}
            Err(mpsc::TryRecvError::Disconnected) => {
                self.network_info_rx = None;
                self.network_info_error =
                    Some("Network loader stopped before reporting a result".into());
            }
        }
    }
    pub fn poll_file_operations(&mut self, actions: &mut Vec<action::AppAction>) {
        if let Some(receiver) = &self.import_rx {
            match receiver.try_recv() {
                Ok(result) => {
                    self.import_rx = None;
                    match result {
                        Ok(board) => {
                            self.settings.builder.boards.push(board);
                            self.queue_builder_update(actions);
                        }
                        Err(error) => {
                            self.operation_error =
                                Some(format!("Failed to import board file: {error}"))
                        }
                    }
                }
                Err(mpsc::TryRecvError::Disconnected) => {
                    self.import_rx = None;
                    self.operation_error =
                        Some("Board import stopped before reporting a result".into());
                }
                Err(mpsc::TryRecvError::Empty) => {}
            }
        }
        if let Some(receiver) = &self.export_rx {
            match receiver.try_recv() {
                Ok(result) => {
                    self.export_rx = None;
                    match result {
                        Ok(()) => self.operation_error = None,
                        Err(error) => {
                            self.operation_error =
                                Some(format!("Failed to export FIL network: {error}"))
                        }
                    }
                }
                Err(mpsc::TryRecvError::Disconnected) => {
                    self.export_rx = None;
                    self.operation_error =
                        Some("Network export stopped before reporting a result".into());
                }
                Err(mpsc::TryRecvError::Empty) => {}
            }
        }
    }
    pub fn has_pending_file_operation(&self) -> bool {
        self.network_info_rx.is_some() || self.import_rx.is_some() || self.export_rx.is_some()
    }
}

