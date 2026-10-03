use super::{
    CanThreadCommand as Command, CanThreadConfig, CanThreadEvent as Event,
    connection::ConnectionManager, decode::FrameDecoder, events::Events, tx::SendTable,
};
use crate::can_thread;
#[cfg(test)]
use crate::connection;
use crate::frame;
use crate::hil;
use crate::{
    Time,
    can::{bus_load::BusLoadTracker, driver::DriverError, logger::DaqLogger},
};
use std::{
    sync::mpsc::{Receiver, RecvTimeoutError, Sender, TryRecvError},
    time::{Duration, Instant},
};
pub(super) fn run(config: CanThreadConfig, out: Sender<Event>, commands: Receiver<Command>) {
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
                    let id = message.msg_id;
                    if let Err(error) = sends.add(message) {
                        sends.delete(id);
                        emit!(Event::SendFailed {
                            msg_id: id,
                            error,
                            retrying: false
                        });
                    }
                }
                Command::DeleteSendMessage { msg_id } => sends.delete(msg_id),
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
                let id = frame.msg_id;
                match connection.write(frame) {
                    Ok(()) => {
                        let amount_left = sends.commit(id, Instant::now());
                        emit!(Event::MessageSent {
                            msg_id: id,
                            timestamp: Time::now(),
                            amount_left
                        });
                    }
                    Err(error) => {
                        emit!(Event::SendFailed {
                            msg_id: id,
                            error: error.to_string(),
                            retrying: !matches!(error, DriverError::Unsupported(_)),
                        });
                        if matches!(error, DriverError::Unsupported(_)) {
                            sends.delete(id);
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
        if connection.connected() {
            match connection.read() {
                Ok(frames) => {
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
        let wait = if connection.connected() {
            Duration::from_millis(2)
        } else {
            Duration::from_millis(50)
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
#[cfg(test)]
mod tests {
    use super::*;
    use crate::can_thread::{AddSendMessage, SendAmount, spawn_can_thread};
    #[test]
    fn loopback_sends_before_receiving_and_stops() {
        let (tx, rx) = std::sync::mpsc::channel();
        let mut handle = spawn_can_thread(CanThreadConfig::default(), tx).unwrap();
        assert!(
            handle
                .command(Command::Connect(Some(
                    connection::ConnectionSource::Loopback
                )))
                .is_ok()
        );
        assert!(
            handle
                .command(Command::AddSendMessage(AddSendMessage {
                    amount: SendAmount::Once,
                    msg_id: 3,
                    is_msg_id_extended: true,
                    msg_bytes: vec![1, 2]
                }))
                .is_ok()
        );
        let mut sent = false;
        loop {
            match rx.recv_timeout(Duration::from_secs(2)).unwrap() {
                Event::MessageSent { amount_left, .. } => {
                    assert!(amount_left.is_none());
                    sent = true;
                }
                Event::Frame(f) => {
                    assert!(sent);
                    assert!(f.is_msg_id_extended);
                    assert_eq!(f.raw_bytes, [1, 2]);
                    break;
                }
                _ => {}
            }
        }
        let now = Instant::now();
        handle.stop().unwrap();
        assert!(now.elapsed() < Duration::from_millis(500));
    }
    #[test]
    fn missing_event_receiver_exits() {
        let (tx, rx) = std::sync::mpsc::channel();
        let handle = spawn_can_thread(CanThreadConfig::default(), tx).unwrap();
        drop(rx);
        let sender = handle.sender();
        let deadline = Instant::now() + Duration::from_secs(2);
        while sender
            .send(Command::DeleteSendMessage { msg_id: 0 })
            .is_ok()
        {
            assert!(Instant::now() < deadline);
            std::thread::sleep(Duration::from_millis(20));
        }
    }
}

#[cfg(test)]
mod failure_tests {
    use super::*;
    use crate::{
        can::driver::{Driver, DriverResult},
        frame::CanFrame,
    };
    struct FailingDriver {
        unsupported: bool,
        closed: Sender<()>,
    }
    impl Driver for FailingDriver {
        fn read_frames(&mut self) -> DriverResult<Vec<CanFrame>> {
            Err(DriverError::Timeout)
        }
        fn write_frame(&mut self, _: CanFrame) -> DriverResult<()> {
            if self.unsupported {
                Err(DriverError::Unsupported("read-only".into()))
            } else {
                Err(DriverError::Write("uncertain write".into()))
            }
        }
        fn close(&mut self) -> DriverResult<()> {
            let _ = self.closed.send(());
            Ok(())
        }
    }
    #[test]
    fn write_failures_report_retryability_and_close_driver() {
        for unsupported in [false, true] {
            let (out, events) = std::sync::mpsc::channel();
            let (commands, input) = std::sync::mpsc::channel();
            let (closed, close_events) = std::sync::mpsc::channel();
            commands
                .send(Command::AddSendMessage(can_thread::AddSendMessage {
                    amount: can_thread::SendAmount::Once,
                    msg_id: 1,
                    is_msg_id_extended: false,
                    msg_bytes: vec![1],
                }))
                .unwrap();
            let worker = std::thread::spawn(move || {
                run_with_connection(
                    CanThreadConfig::default(),
                    out,
                    input,
                    ConnectionManager::with_driver(Box::new(FailingDriver {
                        unsupported,
                        closed,
                    })),
                )
            });
            match events.recv_timeout(Duration::from_secs(2)).unwrap() {
                Event::SendFailed {
                    msg_id, retrying, ..
                } => {
                    assert_eq!(msg_id, 1);
                    assert_eq!(retrying, !unsupported);
                }
                _ => panic!("expected send failure before connection failure"),
            }
            drop(commands);
            worker.join().unwrap();
            close_events.recv_timeout(Duration::from_secs(1)).unwrap();
            assert!(
                !events
                    .try_iter()
                    .any(|e| matches!(e, Event::MessageSent { .. }))
            );
        }
    }
    #[test]
    fn command_channel_closure_stops_disconnected_worker() {
        let (out, _) = std::sync::mpsc::channel();
        let (commands, input) = std::sync::mpsc::channel();
        drop(commands);
        let start = Instant::now();
        run(CanThreadConfig::default(), out, input);
        assert!(start.elapsed() < Duration::from_millis(500));
    }
}

#[cfg(test)]
mod udp_tests {
    use super::*;
    #[test]
    fn idle_udp_rejects_sends_and_stops_promptly() {
        let (tx, rx) = std::sync::mpsc::channel();
        let mut worker = can_thread::spawn_can_thread(CanThreadConfig::default(), tx).unwrap();
        worker
            .command(Command::Connect(Some(connection::ConnectionSource::Udp(0))))
            .unwrap();
        loop {
            match rx.recv_timeout(Duration::from_secs(2)).unwrap() {
                Event::ConnectionSuccessful => break,
                Event::ConnectionFailed(error) => panic!("UDP connection: {error}"),
                _ => {}
            }
        }
        worker
            .command(Command::AddSendMessage(can_thread::AddSendMessage {
                amount: can_thread::SendAmount::Once,
                msg_id: 1,
                is_msg_id_extended: false,
                msg_bytes: vec![1],
            }))
            .unwrap();
        loop {
            match rx.recv_timeout(Duration::from_secs(2)).unwrap() {
                Event::SendFailed { retrying, .. } => {
                    assert!(!retrying);
                    break;
                }
                Event::MessageSent { .. } => panic!("UDP falsely acknowledged a write"),
                _ => {}
            }
        }
        let now = Instant::now();
        worker.stop().unwrap();
        assert!(now.elapsed() < Duration::from_millis(500));
    }
}
