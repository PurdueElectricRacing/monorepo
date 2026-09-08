//! CAN-thread integration. This thread owns the driver and firmware updater;
//! updates pause scheduled application traffic.

use crate::{can, connection, messages, util};

const NO_CONNECTION_SLEEP_MS: u64 = 200;
const READ_RETRY_SLEEP_MS: u64 = 2;
const BUS_LOAD_UPDATE_MS: u128 = 200;
// Keep each burst below the target's 15-frame software queue capacity.
const FIRMWARE_FRAMES_PER_TICK: usize = 8;

// Driver acceptance is not target acknowledgement; synchronization comes from
// START and CRC responses rather than replies to individual data words.
fn send_firmware_frame(
    driver: &mut dyn can::driver::Driver,
    outbound: can::bootloader::OutboundFrame,
) -> can::driver::DriverResult<()> {
    let id = slcan::StandardId::new(outbound.id as u16).ok_or_else(|| {
        can::driver::DriverError::WriteError(format!("invalid bootloader ID 0x{:X}", outbound.id))
    })?;
    let frame =
        slcan::Can2Frame::new_data(slcan::Id::Standard(id), &outbound.data).ok_or_else(|| {
            can::driver::DriverError::WriteError("invalid bootloader payload".to_string())
        })?;
    driver.write_frame(slcan::CanFrame::Can2(frame))
}

fn process_can_frame(frame: &slcan::CanFrame, state: &can::state::State) -> usize {
    match frame {
        slcan::CanFrame::Can2(frame2) => {
            let decode_msg_id = util::can::slcan_to_u32_with_extid_flag(&frame2.id());
            let raw_msg_id = util::can::slcan_to_u32_without_extid_flag(&frame2.id());

            let data = frame2.data().unwrap_or(&[]);
            let timestamp = chrono::Local::now();
            let raw_bytes = data.to_vec();

            let decoded = state
                .parser
                .as_ref()
                .and_then(|parser| parser.decode_msg(decode_msg_id, data));

            match decoded {
                Some(decoded) => {
                    let parsed_msg = messages::ParsedMessage {
                        timestamp,
                        raw_bytes,
                        decoded,
                    };
                    state
                        .can_to_ui_tx
                        .send(messages::MsgFromCan::ParsedMessage(parsed_msg))
                        .expect("Failed to send parsed CAN message");
                }
                None => {
                    if state.parser.is_some() {
                        log::error!(
                            "Failed to parse: frame ID 0x{:X} ({}), data: {:02X?}",
                            raw_msg_id,
                            raw_msg_id,
                            data
                        );
                    } else {
                        log::warn!(
                            "No DBC loaded. Received frame ID 0x{:X} ({}), data: {:02X?}",
                            raw_msg_id,
                            raw_msg_id,
                            data
                        );
                    }

                    let unparsed_msg = messages::UnparsedMessage {
                        timestamp,
                        raw_bytes,
                        msg_id: raw_msg_id,
                    };
                    state
                        .can_to_ui_tx
                        .send(messages::MsgFromCan::UnparsedMessage(unparsed_msg))
                        .expect("Failed to send unparsed CAN message");
                }
            }

            data.len()
        }

        slcan::CanFrame::CanFd(frame_fd) => {
            let msg_id_raw = util::can::slcan_to_u32_without_extid_flag(&frame_fd.id());
            log::warn!(
                "Received CAN FD frame id=0x{:X} len={}",
                msg_id_raw,
                frame_fd.data().len()
            );
            frame_fd.data().len()
        }
    }
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
                    messages::MsgFromUi::DbcSelected(path) => {
                        match can_decode::Parser::from_dbc_file(&path) {
                            Ok(parser) => {
                                state.parser = Some(parser);
                                log::info!("Loaded DBC from {:?}", path);
                            }
                            Err(e) => log::error!("Failed to load DBC {:?}: {e}", path),
                        }
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
                    messages::MsgFromUi::StartFirmwareUpdate(package, bus) => {
                        state.start_firmware_update(package, bus);
                    }
                    messages::MsgFromUi::CancelFirmwareUpdate => {
                        state.cancel_firmware_update();
                    }
                }
            }
            if !state.firmware_update_active() {
                let msgs_to_send = state.send_this_tick();
                for msg in msgs_to_send {
                    if let Some(ref mut active_driver) = state.driver {
                        let id = if msg.is_msg_id_extended {
                            slcan::ExtendedId::new(msg.msg_id & util::can::EXTENDED_ID_MASK)
                                .map(slcan::Id::Extended)
                        } else if msg.msg_id <= util::can::STANDARD_ID_MASK {
                            slcan::StandardId::new(msg.msg_id as u16).map(slcan::Id::Standard)
                        } else {
                            log::warn!(
                                "Invalid message ID {} for sending CAN frame (exceeds 11 bits for standard)",
                                msg.msg_id
                            );
                            None
                        };
                        if let Some(id) = id {
                            if let Some(can2_frame) = slcan::Can2Frame::new_data(id, &msg.msg_bytes)
                            {
                                let frame = slcan::CanFrame::Can2(can2_frame);
                                match active_driver.write_frame(frame) {
                                    Ok(_) => {
                                        log::info!(
                                            "Sent CAN frame with ID 0x{:X} ({}), data: {:02X?}",
                                            msg.msg_id,
                                            msg.msg_id,
                                            msg.msg_bytes
                                        );
                                        state
                                            .can_to_ui_tx
                                            .send(messages::MsgFromCan::MessageSent {
                                                msg_id: msg.msg_id,
                                                timestamp: chrono::Local::now(),
                                                amount_left: state
                                                    .send_msgs
                                                    .get(&msg.msg_id)
                                                    .map(|info| info.amount),
                                                // If the message is removed after the send, this
                                                // will return None, which is what we want to indicate
                                                // no more sends left
                                            })
                                            .expect("Failed to send message sent confirmation");
                                    }
                                    Err(e) => {
                                        log::error!("Failed to send CAN frame: {:?}", e);
                                        state.is_connected = false;
                                        if let Some(ref source) = state.current_source {
                                            let error_msg = source.display_name();
                                            state
                                                .can_to_ui_tx
                                                .send(messages::MsgFromCan::ConnectionFailed(
                                                    error_msg,
                                                ))
                                                .expect("Failed to send connection failed message");
                                        }
                                        state.driver = None;
                                    }
                                }
                            } else {
                                log::error!(
                                    "Cannot send CAN frame: data length {} exceeds 8 bytes",
                                    msg.msg_bytes.len()
                                );
                                continue;
                            }
                        } else {
                            log::warn!("Invalid message ID {} for sending CAN frame", msg.msg_id);
                        }
                    } else {
                        log::warn!("Cannot send CAN frame, no active connection");
                    }
                }
            }

            // Attempt to connect if we don't have a driver but have a source
            if state.driver.is_none() {
                if let Some(ref source) = state.current_source {
                    match can::driver::create_driver(source) {
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
                        // The updater filters for the active board's response ID.
                        let data_bytes = if state.firmware_update_active() {
                            match &frame {
                                slcan::CanFrame::Can2(frame2) => {
                                    let id =
                                        util::can::slcan_to_u32_without_extid_flag(&frame2.id());
                                    state.firmware_frame_received(id, frame2.data().unwrap_or(&[]));
                                    frame2.data().map_or(0, |data| data.len())
                                }
                                slcan::CanFrame::CanFd(frame_fd) => frame_fd.data().len(),
                            }
                        } else {
                            process_can_frame(&frame, &state)
                        };
                        state.bus_load_tracker.record_frame(data_bytes);

                        // Log each frame (buffered, not flushed yet)
                        match &frame {
                            slcan::CanFrame::Can2(f2) => daq_logger.log_can2_frame(f2, false),
                            // RawFrame is fixed at 8 bytes (CAN 2.0 format); FD frames are not logged
                            slcan::CanFrame::CanFd(_) => {}
                        }
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

        unreachable!("CAN thread should never exit on its own");
    })
}
