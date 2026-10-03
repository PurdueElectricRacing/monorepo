use crate::{can, connection, messages};

const NO_CONNECTION_SLEEP_MS: u64 = 200;
const READ_RETRY_SLEEP_MS: u64 = 2;
const BUS_LOAD_UPDATE_MS: u128 = 200;
const HIL_UPDATE_MS: u128 = 50;

// Keep each burst below the target's 15-frame software queue capacity.
const FIRMWARE_FRAMES_PER_TICK: usize = 8;
// The target performs flash writes in its main loop and does not acknowledge
// individual words, so pace serial bursts to avoid overflowing its RX queue.
const FIRMWARE_FRAME_DELAY_MS: u64 = 4;

// Driver acceptance is not target acknowledgement; synchronization comes from
// START and CRC responses rather than replies to individual data words.
fn send_firmware_frame(
    driver: &mut dyn can::driver::Driver,
    outbound: can::bootloader::OutboundFrame,
) -> can::driver::DriverResult<()> {
    let id = daqcore::can::MessageId::from_parts(false, outbound.id as u32)
        .map_err(|e| can::driver::DriverError::WriteError(e.to_string()))?;
    let frame = daqcore::can::CanFrame::new(id, &outbound.data)
        .map_err(|e| can::driver::DriverError::WriteError(e.to_string()))?;
    driver.write_frame(frame)
}

fn process_can_frame(frame: &daqcore::can::CanFrame, state: &mut can::state::State) -> usize {
    let timestamp = chrono::Local::now();
    let bus = frame.bus.unwrap_or(state.active_bus);
    let decoded = state
        .parser
        .as_ref()
        .and_then(|p| daqcore::superdbc::Decoder::new(p.database(), bus))
        .and_then(|decoder| decoder.decode_frame(frame).ok().flatten());
    if let Some(frame) = decoded {
        let parsed = messages::ParsedMessage { timestamp, frame };
        if let Some(db) = &state.parser {
            state.hil_engine.process_parsed(&parsed, db.database());
        }
        let _ = state
            .can_to_ui_tx
            .send(messages::MsgFromCan::ParsedMessage(parsed));
    } else {
        let _ = state
            .can_to_ui_tx
            .send(messages::MsgFromCan::UnparsedMessage(
                messages::UnparsedMessage {
                    timestamp,
                    frame: frame.with_bus(bus),
                },
            ));
    }
    frame.data().len()
}

pub fn start_can_thread(
    can_to_ui_tx: std::sync::mpsc::Sender<messages::MsgFromCan>,
    ui_to_can_rx: std::sync::mpsc::Receiver<messages::MsgFromUi>,
    selected_source: Option<connection::ConnectionSource>,
    log_folder: std::path::PathBuf,
) -> std::thread::JoinHandle<()> {
    std::thread::spawn(move || {
        let mut state = can::state::State::new(can_to_ui_tx, ui_to_can_rx, selected_source);
        let mut daq_logger = can::daq_parser::DaqLogger::new(log_folder);

        // MAIN LOOP
        loop {
            // Process UI messages first (DBC load, new message to send, etc.)
            while let Ok(msg) = state.ui_to_can_rx.try_recv() {
                match msg {
                    messages::MsgFromUi::DatabaseSelected(database) => {
                        state.activate_database(database);
                    }
                    messages::MsgFromUi::Connect(source) => {
                        // Never resume an update on a different physical bus.
                        state.cancel_firmware_update();
                        // Close existing connection if any
                        if let Some(mut old_driver) = state.driver.take() {
                            let _ = old_driver.close();
                        }
                        state.is_connected = false;
                        state
                            .can_to_ui_tx
                            .send(messages::MsgFromCan::Disconnection)
                            .expect("Failed to send disconnected message");
                        state.current_source = Some(source);
                    }
                    messages::MsgFromUi::AddSendMessage(add_send_msg) => {
                        state.add_send_message(add_send_msg);
                    }
                    messages::MsgFromUi::DeleteSendMessage { msg_id } => {
                        state.delete_send_message(msg_id);
                    }
                    messages::MsgFromUi::UpdateLogFolder(path) => {
                        daq_logger.update_folder(path);
                    }
                    messages::MsgFromUi::Hil(command) => {
                        state
                            .hil_engine
                            .handle_command(command, state.parser.as_ref());
                        state.hil_finished_sent = false;
                        state
                            .can_to_ui_tx
                            .send(messages::MsgFromCan::Hil(state.hil_engine.snapshot()))
                            .expect("Failed to send HIL snapshot");
                        state.last_hil_update = std::time::Instant::now();
                    }
                    messages::MsgFromUi::StartFirmwareUpdate(package) => {
                        state.start_firmware_update(package, false);
                    }
                    messages::MsgFromUi::ArmFirmwareUpdate(package) => {
                        state.start_firmware_update(package, true);
                    }
                    messages::MsgFromUi::CancelFirmwareUpdate => {
                        state.cancel_firmware_update();
                    }
                }
            }

            state.hil_engine.tick();
            if state.hil_engine.is_running()
                && !state.hil_finished_sent
                && state.last_hil_update.elapsed().as_millis() >= HIL_UPDATE_MS
            {
                state
                    .can_to_ui_tx
                    .send(messages::MsgFromCan::Hil(state.hil_engine.snapshot()))
                    .expect("Failed to send HIL snapshot");
                state.last_hil_update = std::time::Instant::now();
                state.hil_finished_sent = state.hil_engine.all_finished();
            }

            if !state.firmware_update_active() {
                for msg in state.send_this_tick() {
                    let Some(driver) = state.driver.as_mut() else {
                        continue;
                    };
                    let Ok(id) = daqcore::can::MessageId::from_wire_u32(msg.msg_id) else {
                        continue;
                    };
                    let Ok(frame) = daqcore::can::CanFrame::new(id, &msg.msg_bytes) else {
                        continue;
                    };
                    match driver.write_frame(frame) {
                        Ok(()) => {
                            let _ = state.can_to_ui_tx.send(messages::MsgFromCan::MessageSent {
                                msg_id: msg.msg_id,
                                timestamp: chrono::Local::now(),
                                amount_left: state
                                    .send_msgs
                                    .get(&msg.msg_id)
                                    .map(|info| info.amount),
                            });
                        }
                        Err(e) => {
                            log::error!("Failed to send CAN frame: {e:?}");
                            state.is_connected = false;
                            state.driver = None;
                            let _ = state
                                .can_to_ui_tx
                                .send(messages::MsgFromCan::ConnectionFailed(format!("{e:?}")));
                        }
                    }
                }
            }

            // Attempt to connect if we don't have a driver but have a source
            if state.driver.is_none() {
                if let Some(ref source) = state.current_source {
                    match can::driver::create_driver(source, state.parser.clone()) {
                        Ok(new_driver) => {
                            state.driver = Some(new_driver);
                            state.is_connected = true;
                            state
                                .can_to_ui_tx
                                .send(messages::MsgFromCan::ConnectionSuccessful)
                                .expect("Failed to send connection successful message");
                            daq_logger.reset_start_time();
                            log::info!("Connected to {:?}", source);
                        }
                        Err(e) => {
                            log::error!("Failed to create driver for {:?}: {:?}", source, e);
                            let error_msg = source.display_name();
                            state
                                .can_to_ui_tx
                                .send(messages::MsgFromCan::ConnectionFailed(error_msg))
                                .expect("Failed to send connection failed message");
                            std::thread::sleep(std::time::Duration::from_millis(
                                NO_CONNECTION_SLEEP_MS,
                            ));
                            continue;
                        }
                    }
                } else {
                    // No source configured, just sleep
                    std::thread::sleep(std::time::Duration::from_millis(NO_CONNECTION_SLEEP_MS));
                    continue;
                }
            }

            if state.firmware_update_active() {
                // Batching avoids one adapter read timeout per firmware word.
                for _ in 0..FIRMWARE_FRAMES_PER_TICK {
                    let Some(outbound) = state.firmware_tick() else {
                        break;
                    };
                    if let Some(active_driver) = state.driver.as_mut() {
                        if let Err(error) = send_firmware_frame(active_driver.as_mut(), outbound) {
                            log::error!("Firmware update write failed: {:?}", error);
                            // A write failure means the updater cannot know
                            // which boundary the node observed. Stop rather
                            // than continuing with a possibly shifted index.
                            state.cancel_firmware_update();
                            break;
                        }
                        std::thread::sleep(std::time::Duration::from_millis(
                            FIRMWARE_FRAME_DELAY_MS,
                        ));
                    } else {
                        break;
                    }
                }
            }

            // Try to read a frame from the driver
            let read_result = if let Some(active_driver) = state.driver.as_mut() {
                active_driver.read_frames()
            } else {
                std::thread::sleep(std::time::Duration::from_millis(NO_CONNECTION_SLEEP_MS));
                continue;
            };

            match read_result {
                Ok(frames) => {
                    for frame in frames {
                        let data_bytes = if state.firmware_update_active() {
                            if !frame.id.is_extended() && !frame.is_remote() {
                                state.firmware_frame_received(frame.id.raw(), frame.data());
                            }
                            frame.data().len()
                        } else {
                            process_can_frame(&frame, &mut state)
                        };
                        state.bus_load_tracker.record_frame(data_bytes);
                        daq_logger.log_frame(&frame, state.active_bus);
                    }

                    // Send bus load updates periodically
                    if state.last_bus_load_update.elapsed().as_millis() >= BUS_LOAD_UPDATE_MS {
                        state.bus_load_tracker.cleanup();
                        let can_bus_speed = state
                            .driver
                            .as_ref()
                            .and_then(|d| d.bus_speed())
                            .unwrap_or_default();
                        let load_1s = state.bus_load_tracker.get_load(1, can_bus_speed);
                        let load_5s = state.bus_load_tracker.get_load(5, can_bus_speed);
                        let load_10s = state.bus_load_tracker.get_load(10, can_bus_speed);
                        let load_30s = state.bus_load_tracker.get_load(30, can_bus_speed);

                        state
                            .can_to_ui_tx
                            .send(messages::MsgFromCan::BusLoad {
                                load_1s,
                                load_5s,
                                load_10s,
                                load_30s,
                            })
                            .expect("Failed to send bus load message");

                        state.last_bus_load_update = std::time::Instant::now();
                    }
                }
                Err(can::driver::DriverError::ReadError(error_type)) => {
                    match error_type {
                        can::driver::DriverReadError::Timeout => {
                            // Normal timeout, just retry
                            std::thread::sleep(std::time::Duration::from_millis(
                                READ_RETRY_SLEEP_MS,
                            ));
                        }
                        other => {
                            // Preserve updater state while reconnecting to the same source.
                            log::error!("Driver read error: {:?}", other);
                            state.is_connected = false;
                            if let Some(ref source) = state.current_source {
                                let error_msg = source.display_name();
                                state
                                    .can_to_ui_tx
                                    .send(messages::MsgFromCan::ConnectionFailed(error_msg))
                                    .expect("Failed to send connection failed message");
                            }
                            state.driver = None;
                        }
                    }
                }
                Err(e) => {
                    log::error!("Unexpected driver error: {:?}", e);
                    state.is_connected = false;
                    state.driver = None;
                }
            }
        }
    })
}
