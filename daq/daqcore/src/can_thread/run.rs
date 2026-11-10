use crate::{
    can,
    can_thread::{self, DatabaseUpdate, tx::AddSendMessage},
    frame, hil, superdbc,
};
use std::{sync::mpsc, thread, time};

const HIL_UPDATE_INTERVAL: time::Duration = time::Duration::from_millis(50);
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
    let mut database: Option<superdbc::database::Database> = Option::default();
    let mut selected_bus_id = u8::default();
    let mut sends = can_thread::tx::SendTable::default();
    let mut load = can::bus_load::BusLoadTracker::new(time::Instant::now());
    let mut logger = config.log_folder.map(can::logger::DaqLogger::new);
    let mut firmware = can_thread::firmware_session::FirmwareSession::default();
    let mut hil = hil::engine::HilEngine::new(config.hil_dir);
    let mut hil_last = time::Instant::now();
    let mut hil_finished = false;
    let mut last_hil_snapshot_sent = false;
    let mut hil_capture_complete = false;
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
                    let now = time::Instant::now();
                    connection.select(source.clone(), now);
                    load.reset(now);
                    emit!(can_thread::CanThreadEvent::SourceSelected(source));
                    emit!(can_thread::CanThreadEvent::Disconnection);
                }
                can_thread::CanThreadCommand::SelectDatabase {
                    request_id,
                    selection,
                } => {
                    // Load and validate file replacements on the worker
                    // Failures leave both the definitions and bus unchanged.
                    let update: DatabaseUpdate = match &selection {
                        can_thread::DatabaseRequest::Load {
                            path,
                            preferred_bus,
                        } => {
                            let loaded_database = match superdbc::database::Database::load(path)
                                .map_err(|error| error.to_string())
                            {
                                Ok(database) => database,
                                Err(error) => {
                                    emit!(can_thread::CanThreadEvent::DatabaseSelected {
                                        request_id,
                                        result: Err(error)
                                    });
                                    continue;
                                }
                            };

                            selected_bus_id = match preferred_bus
                                .filter(|id| loaded_database.bus_by_id(*id).is_some())
                                .or_else(|| loaded_database.bus_by_id(0).map(|bus| bus.bus_id()))
                                .or_else(|| loaded_database.buses().first().map(|bus| bus.bus_id()))
                                .ok_or_else(|| "SuperDBC contains no selectable buses".to_owned())
                            {
                                Ok(bus_id) => bus_id,
                                Err(error) => {
                                    emit!(can_thread::CanThreadEvent::DatabaseSelected {
                                        request_id,
                                        result: Err(error)
                                    });
                                    continue;
                                }
                            };

                            let loaded_update = can_thread::DatabaseUpdate::Loaded {
                                database: Box::new(loaded_database.clone()),
                                bus_id: selected_bus_id,
                            };

                            database = Some(loaded_database);

                            loaded_update
                        }

                        can_thread::DatabaseRequest::Bus { bus_id } => {
                            if database.is_none() {
                                emit!(can_thread::CanThreadEvent::DatabaseSelected {
                                    request_id,
                                    result: Err("No SuperDBC database is loaded".to_owned())
                                });
                                continue;
                            }
                            if can_thread::decode::bus(database.as_ref(), *bus_id).is_none() {
                                emit!(can_thread::CanThreadEvent::DatabaseSelected {
                                    request_id,
                                    result: Err(format!("SuperDBC has no bus {bus_id}"))
                                });
                                continue;
                            }
                            selected_bus_id = *bus_id;
                            can_thread::DatabaseUpdate::Bus { bus_id: *bus_id }
                        }
                    };

                    sends = can_thread::tx::SendTable::default();
                    let now = time::Instant::now();
                    hil.handle_command(
                        hil::engine::HilCommand::Stop,
                        now,
                        database.as_ref(),
                        selected_bus_id,
                    );
                    hil_finished = false;
                    last_hil_snapshot_sent = false;
                    hil_capture_complete = false;
                    load.reset(now);
                    if let Some(logger) = &mut logger {
                        logger.flush();
                    }
                    emit!(can_thread::CanThreadEvent::DatabaseSelected {
                        request_id,
                        result: Ok(update)
                    });
                    emit!(can_thread::CanThreadEvent::Hil(hil.snapshot(now)));
                    if selected_bus_id > 1 {
                        emit!(can_thread::CanThreadEvent::Diagnostic(format!(
                            "Legacy logging is unavailable for bus {}; only buses 0/1 are supported.",
                            selected_bus_id
                        )));
                    }
                }
                can_thread::CanThreadCommand::AddSendMessage(message) => {
                    let key = message.key;
                    if let Err(error) = sends.add(message) {
                        sends.delete(key);
                        emit!(can_thread::CanThreadEvent::SendFailed {
                            key,
                            error,
                            retrying: false,
                            is_hil: false,
                        });
                    }
                }
                can_thread::CanThreadCommand::DeleteSendMessage { key } => sends.delete(key),
                can_thread::CanThreadCommand::UpdateLogFolder(path) => match &mut logger {
                    Some(logger) => logger.update_folder(path),
                    None => logger = Some(can::logger::DaqLogger::new(path)),
                },
                can_thread::CanThreadCommand::Hil(command) => {
                    let now = time::Instant::now();
                    hil.handle_command(command, now, database.as_ref(), selected_bus_id);
                    if !hil.is_running() {
                        sends.delete_hil();
                    }
                    emit!(can_thread::CanThreadEvent::Hil(hil.snapshot(now)));
                    hil_last = now;
                    hil_capture_complete = false;
                    hil_finished = false;
                    last_hil_snapshot_sent = false;
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
        if let Some(result) = connection.connect(now, &decoder.active_buses()) {
        // if let Some(result) = connection.connect(
        //     now,
        //     can_thread::decode::bus(database.as_ref(), selected_bus_id),
        // ) {
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
            {
            if !hil_capture_complete
            {
                let mut tx_messages: Vec<AddSendMessage> = Vec::new();
                match hil.queue_tx(now, database.as_ref(), selected_bus_id) {
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
                    if let Err(error) = sends.add(message) {
                        emit!(can_thread::CanThreadEvent::SendFailed {
                            identity,
                            error,
                            retrying: false,
                            is_hil: true,
                        });
                    }
                }
            }

            if !last_hil_snapshot_sent && now.duration_since(hil_last) >= HIL_UPDATE_INTERVAL {
                emit!(can_thread::CanThreadEvent::Hil(hil.snapshot(now)));
                hil_last = now;
                if hil_finished {
                    last_hil_snapshot_sent = true;
                }
            }

            hil_capture_complete = hil.capture_complete(now);
            // hil_finished = hil.all_finished();
        }

        // Ordinary sends are suspended while firmware owns transmission.
        let updating = firmware.active();
        if connection.connected() && !updating {
            for (key, tx_frame) in sends.due(now) {
                match connection.write(tx_frame) {
            // for (key, frame) in sends.due(now) {
            //     let identity = frame.identity;
            //     match connection.write(frame) {
                    Ok(()) => {
                        let amount_left = sends.commit(key, time::Instant::now());
                        // let amount_left = sends.commit(key.0, key.1, time::Instant::now());
                        emit!(can_thread::CanThreadEvent::MessageSent {
                            key,
                            timestamp: crate::time::Time::now(),
                            amount_left,
                            slot_index: key.1,
                        });
                    }
                    Err(error) => {
                        let unsupported = matches!(error, can::driver::DriverError::Unsupported(_));

                        emit!(can_thread::CanThreadEvent::SendFailed {
                            key,
                            error: error.to_string(),
                            retrying: !unsupported,
                            is_hil: key.1 > 0,
                        });
                        if unsupported {
                            sends.delete(key);
                            // sends.delete_all(identity);
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
                        let raw_bus_id = decoder.raw_frame_bus(&frame);
                        // Bus ID is 3 bits so 0-7 supported
                        if raw_bus_id <= 7
                            && let Some(logger) = &mut logger
                            && let Err(error) = logger.log_frame(&frame, raw_bus_id)
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

                        let Some(bus_id) = decoder.resolve_frame_bus(&frame) else {
                            continue;
                        };
                        load.record_frame(bus_id, frame.data.len(), time::Instant::now());

                        let frame = can_thread::decode::decode(
                            frame,
                            bus_id, crate::time::Time::now(),
                            database.as_ref(),
                            selected_bus_id,
                        );
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

        if let Some(buses) = load.poll(
            &decoder.active_buses(),
            connection.driver_speed(),
            time::Instant::now(),
        ) {
            emit!(can_thread::CanThreadEvent::BusLoad {
                timestamp: crate::time::Time::now(),
                buses,
            });
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
