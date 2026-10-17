use crate::{
    can,
    can_thread::{self, DatabaseUpdate, tx::AddSendMessage},
    frame, hil,
    superdbc::database::Bus,
};
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
    let mut hil_finished = false;
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
                    match &update {
                        DatabaseUpdate::Loaded { database, bus_id } => {
                            hil.set_decoder((**database).clone(), *bus_id)
                        }
                        DatabaseUpdate::Bus { bus_id } => hil.set_bus_id(*bus_id),
                    }
                    sends = can_thread::tx::SendTable::default();
                    let now = time::Instant::now();
                    hil.handle_command(hil::engine::HilCommand::Stop, now);
                    hil_finished = false;
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
                    if decoder.bus_id() > 1 {
                        emit!(can_thread::CanThreadEvent::Diagnostic(format!(
                            "Legacy logging is unavailable for bus {}; only buses 0/1 are supported.",
                            decoder.bus_id()
                        )));
                    }
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
                    hil_finished = false;
                }
                can_thread::CanThreadCommand::StartFirmwareUpdate(package) => {
                    let progress = firmware.start(
                        package,
                        false,
                        connection.connected(),
                        time::Instant::now(),
                    );
                    emit!(can_thread::CanThreadEvent::FirmwareProgress(progress));
                }
                can_thread::CanThreadCommand::ArmFirmwareUpdate(package) => {
                    let progress =
                        firmware.start(package, true, connection.connected(), time::Instant::now());
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
        if hil.is_running() && !hil_finished && now.duration_since(hil_last) >= HIL_UPDATE_INTERVAL
        {
            let mut tx_messages: Vec<AddSendMessage> = Vec::new();
            match hil.queue_tx(now) {
                Ok((messages, errors)) => {
                    tx_messages = messages;
                    for error in errors {
                        emit!(can_thread::CanThreadEvent::Diagnostic(error));
                    }
                }
                Err(error) => emit!(can_thread::CanThreadEvent::Diagnostic(error)),
            }
            for message in tx_messages {
                let identity = message.identity;
                if !sends.contains(identity) {
                    if let Err(error) = sends.add(message) {
                        sends.delete(identity);
                        emit!(can_thread::CanThreadEvent::SendFailed {
                            identity,
                            error,
                            retrying: false
                        });
                    }
                } else {
                    emit!(can_thread::CanThreadEvent::Diagnostic(
                        "Attempted HIL message is already being sent".to_owned()
                    ));
                }
            }
            emit!(can_thread::CanThreadEvent::Hil(hil.snapshot(now)));
            hil_last = now;
            hil_finished = hil.all_finished();
        }

        // Ordinary sends are suspended while firmware owns transmission.
        let updating = firmware.active();
        if connection.connected() && !updating {
            for frame in sends.due(now) {
                let identity = frame.identity;
                match connection.write(frame) {
                    Ok(()) => {
                        let amount_left = sends.commit(identity, time::Instant::now());
                        emit!(can_thread::CanThreadEvent::MessageSent {
                            identity,
                            timestamp: crate::time::Time::now(),
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

                let frame = frame::CanIdentity::new(frame.id, false)
                    .map_err(|error| error.to_string())
                    .and_then(|identity| frame::CanFrame::data(identity, frame.data));
                let result = frame
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
                        if decoder.bus_id() <= 1
                            && let Some(logger) = &mut logger
                            && let Err(error) = logger.log_frame(&frame, decoder.bus_id())
                        {
                            emit!(can_thread::CanThreadEvent::Diagnostic(error));
                        }

                        let updating = firmware.active();
                        if updating
                            && !frame.identity.is_extended()
                            && let Some(progress) = firmware.receive(
                                frame.identity.raw_id(),
                                &frame.data,
                                time::Instant::now(),
                            )
                        {
                            emit!(can_thread::CanThreadEvent::FirmwareProgress(progress));
                        }

                        let frame = decoder.decode(frame, crate::time::Time::now());
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
                timestamp: crate::time::Time::now(),
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
