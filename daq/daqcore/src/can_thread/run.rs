use crate::{
    Time,
    can::{bus_load::BusLoadTracker, driver::DriverError, logger::DaqLogger},
    can_thread::{
        self, CanThreadCommand as Command, CanThreadConfig, CanThreadEvent as Event,
        connection::ConnectionManager, decode::FrameDecoder, events::Events, tx::SendTable,
    },
    frame, hil,
};

use std::{
    sync::mpsc::{Receiver, RecvTimeoutError, Sender, TryRecvError},
    time::{Duration, Instant},
};
pub fn run(config: CanThreadConfig, out: Sender<Event>, commands: Receiver<Command>) {
    run_with_connection(
        config,
        out,
        commands,
        ConnectionManager::new(Instant::now()),
    );
}
fn run_with_connection(
    config: CanThreadConfig,
    out: Sender<Event>,
    commands: Receiver<Command>,
    mut connection: ConnectionManager,
) {
    let events = Events(out);
    let mut decoder = FrameDecoder::default();
    let mut sends = SendTable::default();
    let mut load = BusLoadTracker::default();
    let mut logger = config.log_folder.map(DaqLogger::new);
    let mut firmware = can_thread::firmware_session::FirmwareSession::default();
    let mut hil = hil::engine::HilEngine::new(config.hil_dir);
    let mut hil_last = Instant::now();
    let mut hil_finished = false;
    let mut load_last = Instant::now();
    let mut pending = None;
    if let Some(path) = config.dbc_path {
        if let Err(error) = decoder.reload(&path) {
            if !events.emit(Event::Diagnostic(error)) {
                return;
            }
        }
    }
    'worker: loop {
        macro_rules! emit {
            ($event:expr) => {
                if !events.emit($event) {
                    break 'worker;
                }
            };
        }
        loop {
            let command = match pending
                .take()
                .map(Ok)
                .unwrap_or_else(|| commands.try_recv())
            {
                Ok(c) => c,
                Err(TryRecvError::Empty) => break,
                Err(TryRecvError::Disconnected) => break 'worker,
            };
            match command {
                Command::Stop => break 'worker,
                Command::Connect(source) => {
                    if let Some(p) = firmware.cancel() {
                        emit!(Event::FirmwareProgress(p));
                    }
                    connection.select(source.clone(), Instant::now());
                    load = BusLoadTracker::default();
                    emit!(Event::SourceSelected(source));
                    emit!(Event::Disconnection);
                }
                Command::DbcSelected(path) => {
                    if let Err(e) = decoder.reload(&path) {
                        emit!(Event::Diagnostic(e));
                    }
                }
                Command::AddSendMessage(message) => {
                    let identity = message.identity;
                    if let Err(error) = sends.add(message) {
                        sends.delete(identity);
                        emit!(Event::SendFailed {
                            identity,
                            error,
                            retrying: false
                        });
                    }
                }
                Command::DeleteSendMessage { identity } => sends.delete(identity),
                Command::UpdateLogFolder(path) => match &mut logger {
                    Some(logger) => logger.update_folder(path),
                    None => logger = Some(DaqLogger::new(path)),
                },
                Command::Hil(command) => {
                    let now = Instant::now();
                    hil.handle_command(command, now);
                    emit!(Event::Hil(hil.snapshot(now)));
                    hil_last = now;
                    hil_finished = false;
                }
                Command::StartFirmwareUpdate(package) => {
                    let p = firmware.start(package, false, connection.connected(), Instant::now());
                    emit!(Event::FirmwareProgress(p));
                }
                Command::ArmFirmwareUpdate(package) => {
                    let p = firmware.start(package, true, connection.connected(), Instant::now());
                    emit!(Event::FirmwareProgress(p));
                }
                Command::CancelFirmwareUpdate => {
                    if let Some(p) = firmware.cancel() {
                        emit!(Event::FirmwareProgress(p));
                    }
                }
            }
        }
        let now = Instant::now();
        if let Some(result) = connection.connect(now) {
            match result {
                Ok(()) => {
                    if let Some(logger) = &mut logger {
                        logger.reset_start_time();
                    }
                    emit!(Event::ConnectionSuccessful);
                }
                Err(error) => emit!(Event::ConnectionFailed(error)),
            }
        }
        {
            hil.tick(now);
            if hil.is_running()
                && !hil_finished
                && now.duration_since(hil_last) >= Duration::from_millis(50)
            {
                emit!(Event::Hil(hil.snapshot(now)));
                hil_last = now;
                hil_finished = hil.all_finished();
            }
        }
        let updating = firmware.active();
        if connection.connected() && !updating {
            for frame in sends.due(now) {
                let identity = frame.identity();
                match connection.write(frame) {
                    Ok(()) => {
                        let amount_left = sends.commit(identity, Instant::now());
                        emit!(Event::MessageSent {
                            identity,
                            timestamp: Time::now(),
                            amount_left
                        });
                    }
                    Err(error) => {
                        emit!(Event::SendFailed {
                            identity,
                            error: error.to_string(),
                            retrying: !matches!(error, DriverError::Unsupported(_)),
                        });
                        if matches!(error, DriverError::Unsupported(_)) {
                            sends.delete(identity);
                        } else {
                            connection.failed(Instant::now());
                            emit!(Event::ConnectionFailed(error.to_string()));
                            break;
                        }
                    }
                }
            }
        }
        if firmware.active() && connection.connected() {
            for _ in 0..8 {
                let result = firmware.tick(Instant::now());
                if let Some(progress) = result.progress {
                    emit!(Event::FirmwareProgress(progress));
                }
                let Some(frame) = result.frame else {
                    break;
                };
                let frame = frame::CanFrame::data(frame.id, false, frame.data);
                let result = frame
                    .map_err(DriverError::Write)
                    .and_then(|frame| connection.write(frame));
                if let Err(error) = result {
                    if let Some(mut p) = firmware.cancel() {
                        p.error = Some(format!("firmware write failed: {error}"));
                        emit!(Event::FirmwareProgress(p));
                    }
                    connection.failed(Instant::now());
                    emit!(Event::ConnectionFailed(error.to_string()));
                    break;
                }
                std::thread::sleep(Duration::from_millis(4));
            }
        }
        let mut got_frames = false;
        if connection.connected() {
            match connection.read() {
                Ok(frames) => {
                    got_frames = !frames.is_empty();
                    for frame in frames {
                        load.record_frame(frame.data.len(), Instant::now());
                        if let Some(logger) = &mut logger {
                            logger.log_frame(&frame);
                        }
                        let updating = firmware.active();
                        if updating {
                            if !frame.is_msg_id_extended {
                                if let Some(p) =
                                    firmware.receive(frame.msg_id, &frame.data, Instant::now())
                                {
                                    emit!(Event::FirmwareProgress(p));
                                }
                            }
                        }
                        {
                            let frame = decoder.decode(frame, Time::now());
                            if !updating {
                                hil.process_parsed(&frame, Instant::now());
                            }
                            emit!(Event::Frame(frame));
                        }
                    }
                }
                Err(DriverError::Timeout) => {}
                Err(error) => {
                    connection.failed(Instant::now());
                    emit!(Event::ConnectionFailed(error.to_string()));
                }
            }
        }
        let now = Instant::now();
        if now.duration_since(load_last) >= Duration::from_millis(200) {
            load.cleanup(now);
            let speed = connection.speed();
            emit!(Event::BusLoad {
                timestamp: Time::now(),
                load_1s: load.get_load(1, speed, now),
                load_5s: load.get_load(5, speed, now),
                load_10s: load.get_load(10, speed, now),
                load_30s: load.get_load(30, speed, now)
            });
            load_last = now;
        }
        let wait = if !connection.connected() {
            Duration::from_millis(50)
        } else if got_frames {
            // Drain queued traffic without delaying the next read; commands and
            // scheduled work still run between driver batches.
            Duration::ZERO
        } else {
            Duration::from_millis(2)
        };
        match commands.recv_timeout(wait) {
            Ok(command) => pending = Some(command),
            Err(RecvTimeoutError::Disconnected) => break,
            Err(RecvTimeoutError::Timeout) => {}
        }
    }
    if let Some(progress) = firmware.cancel() {
        let _ = events.emit(Event::FirmwareProgress(progress));
    }
    if let Some(logger) = &mut logger {
        logger.flush();
    }
    connection.close();
}
