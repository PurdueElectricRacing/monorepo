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

const HIL_UPDATE_INTERVAL: Duration = Duration::from_millis(50);
const BUS_LOAD_UPDATE_INTERVAL: Duration = Duration::from_millis(200);
const FIRMWARE_FRAMES_PER_BURST: usize = 8;
const FIRMWARE_FRAME_DELAY: Duration = Duration::from_millis(4);

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

    if let Some(path) = config.dbc_path
        && let Err(error) = decoder.reload(&path)
        && !events.emit(Event::Diagnostic(error))
    {
        return;
    }

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
                Err(TryRecvError::Empty) => break,
                Err(TryRecvError::Disconnected) => break 'worker,
            };

            match command {
                Command::Stop => break 'worker,
                Command::Connect(source) => {
                    if let Some(progress) = firmware.cancel() {
                        emit!(Event::FirmwareProgress(progress));
                    }
                    for event in connection.close() {
                        emit!(Event::FilExpectation(event));
                    }
                    connection.select(source.clone(), Instant::now());
                    load = BusLoadTracker::default();
                    emit!(Event::SourceSelected(source));
                    emit!(Event::Disconnection);
                }
                Command::DbcSelected(path) => {
                    if let Err(error) = decoder.reload(&path) {
                        emit!(Event::Diagnostic(error));
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
                    let progress =
                        firmware.start(package, false, connection.connected(), Instant::now());
                    emit!(Event::FirmwareProgress(progress));
                }
                Command::ArmFirmwareUpdate(package) => {
                    let progress =
                        firmware.start(package, true, connection.connected(), Instant::now());
                    emit!(Event::FirmwareProgress(progress));
                }
                Command::CancelFirmwareUpdate => {
                    if let Some(progress) = firmware.cancel() {
                        emit!(Event::FirmwareProgress(progress));
                    }
                }
                Command::SetFilAdc {
                    board,
                    instance,
                    channel,
                    value,
                } => {
                    if let Err(error) = connection.set_adc(&board, &instance, channel, value) {
                        emit!(Event::Diagnostic(error.to_string()));
                    }
                }
                Command::SetFilGpio {
                    board,
                    port,
                    pin,
                    value,
                } => {
                    if let Err(error) = connection.set_gpio(&board, &port, pin, value) {
                        emit!(Event::Diagnostic(error.to_string()));
                    }
                }
                Command::SetFilTraceBus(trace_bus) => {
                    if let Err(error) = connection.set_fil_trace_bus(trace_bus) {
                        emit!(Event::Diagnostic(error.to_string()));
                    }
                }
            }
        }

        // Reconnect the selected source when its retry delay has elapsed.
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

        hil.tick(now);
        if hil.is_running() && !hil_finished && now.duration_since(hil_last) >= HIL_UPDATE_INTERVAL
        {
            emit!(Event::Hil(hil.snapshot(now)));
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
                        let amount_left = sends.commit(identity, Instant::now());
                        emit!(Event::MessageSent {
                            identity,
                            timestamp: Time::now(),
                            amount_left
                        });
                    }
                    Err(error) => {
                        let unsupported = matches!(error, DriverError::Unsupported(_));

                        emit!(Event::SendFailed {
                            identity,
                            error: error.to_string(),
                            retrying: !unsupported,
                        });
                        if unsupported {
                            sends.delete(identity);
                        } else {
                            for event in connection.failed(Instant::now()) {
                                emit!(Event::FilExpectation(event));
                            }
                            emit!(Event::ConnectionFailed(error.to_string()));
                            break;
                        }
                    }
                }
            }
        }

        // Pace firmware words in bounded bursts to protect the target RX queue.
        if firmware.active() && connection.connected() {
            for _ in 0..FIRMWARE_FRAMES_PER_BURST {
                let result = firmware.tick(Instant::now());
                if let Some(progress) = result.progress {
                    emit!(Event::FirmwareProgress(progress));
                }

                let Some(frame) = result.frame else {
                    break;
                };

                let frame = frame::CanIdentity::new(frame.id, false)
                    .map_err(|error| error.to_string())
                    .and_then(|identity| frame::CanFrame::data(identity, frame.data));
                let result = frame
                    .map_err(DriverError::Write)
                    .and_then(|frame| connection.write(frame));
                if let Err(error) = result {
                    if let Some(mut progress) = firmware.cancel() {
                        progress.error = Some(format!("firmware write failed: {error}"));
                        emit!(Event::FirmwareProgress(progress));
                    }
                    for event in connection.failed(Instant::now()) {
                        emit!(Event::FilExpectation(event));
                    }
                    emit!(Event::ConnectionFailed(error.to_string()));
                    break;
                }
                std::thread::sleep(FIRMWARE_FRAME_DELAY);
            }
        }

        for gpio in connection.take_fil_gpio_events() {
            emit!(Event::FilGpio {
                board: gpio.board,
                port: gpio.port,
                pin: gpio.pin,
                value: gpio.value,
                direction: gpio.direction
            });
        }
        for expectation in connection.take_fil_expectation_events() {
            emit!(Event::FilExpectation(expectation));
        }
        // Each received frame is logged, decoded, then moved into an event.
        let mut got_frames = false;
        if connection.connected() {
            match connection.read() {
                Ok(frames) => {
                    got_frames = !frames.is_empty();
                    for frame in frames {
                        load.record_frame(frame.data.len(), Instant::now());
                        if let Some(logger) = &mut logger
                            && let Err(error) = logger.log_frame(&frame)
                        {
                            emit!(Event::Diagnostic(error));
                        }

                        let updating = firmware.active();
                        if updating
                            && !frame.identity.is_extended()
                            && let Some(progress) = firmware.receive(
                                frame.identity.raw_id(),
                                &frame.data,
                                Instant::now(),
                            )
                        {
                            emit!(Event::FirmwareProgress(progress));
                        }

                        let frame = decoder.decode(frame, Time::now());
                        if !updating {
                            hil.process_parsed(&frame, Instant::now());
                        }

                        emit!(Event::Frame(frame));
                    }
                }
                Err(DriverError::Timeout) => {}
                Err(error) => {
                    for event in connection.failed(Instant::now()) {
                        emit!(Event::FilExpectation(event));
                    }
                    emit!(Event::ConnectionFailed(error.to_string()));
                }
            }
        }

        let now = Instant::now();
        if now.duration_since(load_last) >= BUS_LOAD_UPDATE_INTERVAL {
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
        } else if got_frames || !connection.needs_read_retry_sleep() {
            // Drain queued traffic without delaying the next read; FIL already
            // performs its own bounded wait, so avoid stacking another retry delay.
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

    // All shutdown paths cancel operations, flush logs, and close the driver.
    if let Some(progress) = firmware.cancel() {
        let _ = events.emit(Event::FirmwareProgress(progress));
    }

    if let Some(logger) = &mut logger {
        logger.flush();
    }

    for expectation in connection.close() {
        let _ = events.emit(Event::FilExpectation(expectation));
    }
}
