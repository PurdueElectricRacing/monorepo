use crate::{can, can_thread, hil, time::Time};
use std::{sync::mpsc, thread, time};

const HIL_UPDATE_INTERVAL: time::Duration = time::Duration::from_millis(50);
const BUS_LOAD_UPDATE_INTERVAL: time::Duration = time::Duration::from_millis(200);
const FIRMWARE_FRAMES_PER_BURST: usize = 8;
const FIRMWARE_FRAME_DELAY: time::Duration = time::Duration::from_millis(4);

pub fn run(
    config: can_thread::CanThreadConfig,
    out: mpsc::Sender<can_thread::CanThreadEvent>,
    commands: mpsc::Receiver<can_thread::CanThreadCommand>,
) {
    run_with_connection(
        config,
        out,
        commands,
        can_thread::connection::ConnectionManager::new(time::Instant::now()),
    );
}

fn run_with_connection(
    config: can_thread::CanThreadConfig,
    out: mpsc::Sender<can_thread::CanThreadEvent>,
    commands: mpsc::Receiver<can_thread::CanThreadCommand>,
    mut connection: can_thread::connection::ConnectionManager,
) {
    let events = can_thread::events::Events(out);
    let mut decoder = can_thread::decode::FrameDecoder::default();
    let mut sends = can_thread::tx::SendTable::default();
    let mut load = can::bus_load::BusLoadTracker::default();
    let mut logger = config.log_folder.map(can::logger::DaqLogger::new);
    let mut firmware = can_thread::firmware_session::FirmwareSession::default();
    let mut hil = hil::engine::HilEngine::new(config.hil_dir);
    let mut hil_last = time::Instant::now();
    let mut hil_capture_complete = false;
    let mut load_last = time::Instant::now();
    let mut pending = None;

    'worker: loop {
        macro_rules! emit {
            ($event:expr) => {
                if !events.emit($event) {
                    break 'worker;
                }
            };
        }

        // Apply pending commands before advancing scheduled work or reading CAN.
        loop {
            let next_command = match pending.take() {
                Some(command) => Ok(command),
                None => commands.try_recv(),
            };

            let command = match next_command {
                Ok(command) => command,
                Err(mpsc::TryRecvError::Empty) => break,
                Err(mpsc::TryRecvError::Disconnected) => break 'worker,
            };

            match command {
                can_thread::CanThreadCommand::Stop => break 'worker,
                can_thread::CanThreadCommand::Connect(source) => {
                    if let Some(progress) = firmware.cancel() {
                        emit!(can_thread::CanThreadEvent::FirmwareProgress(progress));
                    }
                    connection.select(source.clone(), time::Instant::now());
                    load = can::bus_load::BusLoadTracker::default();
                    emit!(can_thread::CanThreadEvent::SourceSelected(source));
                    emit!(can_thread::CanThreadEvent::Disconnection);
                }
                can_thread::CanThreadCommand::SelectDatabase {
                    request_id,
                    selection,
                } => {
                    if firmware.active() {
                        emit!(can_thread::CanThreadEvent::DatabaseSelected {
                            request_id,
                            result: Err(
                                "cannot change database or bus during a firmware update".into()
                            )
                        });
                        continue;
                    }
                    let update = match decoder.select(&selection) {
                        Ok(update) => update,
                        Err(error) => {
                            emit!(can_thread::CanThreadEvent::DatabaseSelected {
                                request_id,
                                result: Err(error)
                            });
                            continue;
                        }
                    };
                    sends = can_thread::tx::SendTable::default();
                    let now = time::Instant::now();
                    hil.handle_command(hil::engine::HilCommand::Stop, now);
                    hil_capture_complete = false;
                    load = can::bus_load::BusLoadTracker::default();
                    load_last = now;
                    if let Some(logger) = &mut logger {
                        logger.flush();
                    }
                    emit!(can_thread::CanThreadEvent::DatabaseSelected {
                        request_id,
                        result: Ok(update)
                    });
                    emit!(can_thread::CanThreadEvent::Hil(hil.snapshot(now)));
                }
                can_thread::CanThreadCommand::AddSendMessage(message) => {
                    let identity = message.identity;
                    if let Err(error) = sends.add(message) {
                        sends.delete(identity);
                        emit!(can_thread::CanThreadEvent::SendFailed {
                            identity,
                            error,
                            retrying: false
                        });
                    }
                }
                can_thread::CanThreadCommand::DeleteSendMessage { identity } => {
                    sends.delete(identity)
                }
                can_thread::CanThreadCommand::UpdateLogFolder(path) => match &mut logger {
                    Some(logger) => logger.update_folder(path),
                    None => logger = Some(can::logger::DaqLogger::new(path)),
                },
                can_thread::CanThreadCommand::Hil(command) => {
                    let now = time::Instant::now();
                    hil.handle_command(command, now);
                    emit!(can_thread::CanThreadEvent::Hil(hil.snapshot(now)));
                    hil_last = now;
                    hil_capture_complete = false;
                }
                can_thread::CanThreadCommand::StartFirmwareUpdate(package) => {
                    let progress = firmware.start(
                        package,
                        false,
                        connection.connected(),
                        decoder.bus().map(|bus| bus.name()),
                        time::Instant::now(),
                    );
                    emit!(can_thread::CanThreadEvent::FirmwareProgress(progress));
                }
                can_thread::CanThreadCommand::ArmFirmwareUpdate(package) => {
                    let progress = firmware.start(
                        package,
                        true,
                        connection.connected(),
                        decoder.bus().map(|bus| bus.name()),
                        time::Instant::now(),
                    );
                    emit!(can_thread::CanThreadEvent::FirmwareProgress(progress));
                }
                can_thread::CanThreadCommand::CancelFirmwareUpdate => {
                    if let Some(progress) = firmware.cancel() {
                        emit!(can_thread::CanThreadEvent::FirmwareProgress(progress));
                    }
                }
            }
        }

        // Reconnect the selected source when its retry delay has elapsed.
        let now = time::Instant::now();
        if let Some(result) = connection.connect(now, decoder.bus()) {
            match result {
                Ok(()) => {
                    if let Some(logger) = &mut logger {
                        logger.reset_start_time();
                    }
                    emit!(can_thread::CanThreadEvent::ConnectionSuccessful);
                }
                Err(error) => emit!(can_thread::CanThreadEvent::ConnectionFailed(error)),
            }
        }

        hil.tick(now);
        if hil.is_running()
            && !hil_capture_complete
            && now.duration_since(hil_last) >= HIL_UPDATE_INTERVAL
        {
            emit!(can_thread::CanThreadEvent::Hil(hil.snapshot(now)));
            hil_last = now;
            hil_capture_complete = hil.capture_complete(now);
        }

        // Ordinary sends are suspended while firmware owns transmission.
        let updating = firmware.active();
        if connection.connected() && !updating {
            for tx_frame in sends.due(now) {
                let identity = tx_frame.frame.identity;
                match connection.write(tx_frame) {
                    Ok(()) => {
                        let amount_left = sends.commit(identity, time::Instant::now());
                        emit!(can_thread::CanThreadEvent::MessageSent {
                            identity,
                            timestamp: Time::now(),
                            amount_left
                        });
                    }
                    Err(error) => {
                        let unsupported = matches!(error, can::driver::DriverError::Unsupported(_));

                        emit!(can_thread::CanThreadEvent::SendFailed {
                            identity,
                            error: error.to_string(),
                            retrying: !unsupported,
                        });
                        if unsupported {
                            sends.delete(identity);
                        } else {
                            connection.failed(time::Instant::now());
                            emit!(can_thread::CanThreadEvent::ConnectionFailed(
                                error.to_string()
                            ));
                            break;
                        }
                    }
                }
            }
        }

        // Pace firmware words in bounded bursts to protect the target RX queue.
        if firmware.active() && connection.connected() {
            for _ in 0..FIRMWARE_FRAMES_PER_BURST {
                let result = firmware.tick(time::Instant::now());
                if let Some(progress) = result.progress {
                    emit!(can_thread::CanThreadEvent::FirmwareProgress(progress));
                }

                let Some(frame) = result.frame else {
                    break;
                };

                let tx_frame = frame.into_can_frame();
                let result = tx_frame
                    .map_err(can::driver::DriverError::Write)
                    .and_then(|frame| connection.write(frame));
                if let Err(error) = result {
                    if let Some(mut progress) = firmware.cancel() {
                        progress.error = Some(format!("firmware write failed: {error}"));
                        emit!(can_thread::CanThreadEvent::FirmwareProgress(progress));
                    }
                    connection.failed(time::Instant::now());
                    emit!(can_thread::CanThreadEvent::ConnectionFailed(
                        error.to_string()
                    ));
                    break;
                }
                thread::sleep(FIRMWARE_FRAME_DELAY);
            }
        }

        // Each received frame is logged, decoded, then moved into an event.
        let mut got_frames = false;
        if connection.connected() {
            match connection.read() {
                Ok(frames) => {
                    got_frames = !frames.is_empty();
                    for frame in frames {
                        load.record_frame(frame.data.len(), time::Instant::now());
                        // Bus ID is 3 bits so 0-7 supported
                        if decoder.bus_id() <= 7
                            && let Some(logger) = &mut logger
                            && let Err(error) = logger.log_frame(&frame, decoder.bus_id())
                        {
                            emit!(can_thread::CanThreadEvent::Diagnostic(error));
                        }

                        let updating = firmware.active();
                        if updating
                            && let Some(progress) =
                                firmware.receive(frame.identity, &frame.data, time::Instant::now())
                        {
                            emit!(can_thread::CanThreadEvent::FirmwareProgress(progress));
                        }

                        let frame = decoder.decode(frame, Time::now());
                        if let Some(decoded) = &frame.decoded
                            && let Some(progress) =
                                firmware.observe_telemetry(decoded, time::Instant::now())
                        {
                            emit!(can_thread::CanThreadEvent::FirmwareProgress(progress));
                        }
                        if !updating {
                            hil.process_parsed(&frame, time::Instant::now());
                        }

                        emit!(can_thread::CanThreadEvent::Frame(frame));
                    }
                }
                Err(can::driver::DriverError::Timeout) => {}
                Err(error) => {
                    connection.failed(time::Instant::now());
                    emit!(can_thread::CanThreadEvent::ConnectionFailed(
                        error.to_string()
                    ));
                }
            }
        }

        let now = time::Instant::now();
        if now.duration_since(load_last) >= BUS_LOAD_UPDATE_INTERVAL {
            load.cleanup(now);
            let speed = connection.speed();
            emit!(can_thread::CanThreadEvent::BusLoad {
                timestamp: Time::now(),
                load_1s: load.get_load(1, speed, now),
                load_5s: load.get_load(5, speed, now),
                load_10s: load.get_load(10, speed, now),
                load_30s: load.get_load(30, speed, now)
            });
            load_last = now;
        }

        let wait = if !connection.connected() {
            time::Duration::from_millis(50)
        } else if got_frames {
            // Drain queued traffic without delaying the next read; commands and
            // scheduled work still run between driver batches.
            time::Duration::ZERO
        } else {
            time::Duration::from_millis(2)
        };

        match commands.recv_timeout(wait) {
            Ok(command) => pending = Some(command),
            Err(mpsc::RecvTimeoutError::Disconnected) => break,
            Err(mpsc::RecvTimeoutError::Timeout) => {}
        }
    }

    // All shutdown paths cancel operations, flush logs, and close the driver.
    if let Some(progress) = firmware.cancel() {
        let _ = events.emit(can_thread::CanThreadEvent::FirmwareProgress(progress));
    }

    if let Some(logger) = &mut logger {
        logger.flush();
    }

    connection.close();
}
