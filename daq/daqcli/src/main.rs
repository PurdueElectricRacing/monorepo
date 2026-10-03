use daqcore::{
    Session, Time,
    can_thread::{CanThreadCommand, CanThreadConfig, CanThreadEvent, spawn_can_thread},
    connection::{CanBusSpeed, ConnectionSource},
};
use std::{
    sync::{
        atomic::{AtomicBool, Ordering},
        mpsc,
    },
    time::Duration,
};
static STOP: AtomicBool = AtomicBool::new(false);
#[cfg(unix)]
extern "C" fn stop_signal(_: libc::c_int) {
    STOP.store(true, Ordering::Relaxed);
}
#[cfg(windows)]
unsafe extern "system" fn stop_console(kind: u32) -> i32 {
    if kind == 0 || kind == 1 {
        STOP.store(true, Ordering::Relaxed);
        1
    } else {
        0
    }
}
fn install_shutdown() -> Result<(), String> {
    #[cfg(unix)]
    unsafe {
        if libc::signal(libc::SIGINT, stop_signal as *const () as libc::sighandler_t)
            == libc::SIG_ERR
        {
            return Err(std::io::Error::last_os_error().to_string());
        }
        if libc::signal(
            libc::SIGTERM,
            stop_signal as *const () as libc::sighandler_t,
        ) == libc::SIG_ERR
        {
            return Err(std::io::Error::last_os_error().to_string());
        }
    }
    #[cfg(windows)]
    unsafe {
        #[link(name = "Kernel32")]
        unsafe extern "system" {
            fn SetConsoleCtrlHandler(
                handler: Option<unsafe extern "system" fn(u32) -> i32>,
                add: i32,
            ) -> i32;
        }
        if SetConsoleCtrlHandler(Some(stop_console), 1) == 0 {
            return Err(std::io::Error::last_os_error().to_string());
        }
    }
    Ok(())
}
fn usage() {
    println!(
        "daqcli connect|watch --source serial|loopback|udp|simulated [--port DEVICE] [--bitrate 250|500] [--udp-port 5005] [--dbc FILE] [--id DECIMAL|0xHEX]\nCtrl-C closes the connection and joins the worker. Optional sources require the corresponding Cargo feature."
    );
}
fn main() -> Result<(), Box<dyn std::error::Error>> {
    let mut args = std::env::args().skip(1);
    let Some(command) = args.next() else {
        usage();
        return Ok(());
    };
    if command == "--help" || command == "-h" {
        usage();
        return Ok(());
    }
    if command != "watch" && command != "connect" {
        return Err(format!("unknown command: {command}").into());
    }
    let mut source = "loopback".to_string();
    let mut port = None;
    let mut speed = CanBusSpeed::Kbps500;
    let mut udp_port = 5005;
    let mut dbc = None;
    let mut filter = None;
    while let Some(arg) = args.next() {
        if arg == "--help" || arg == "-h" {
            usage();
            return Ok(());
        }
        let value = args
            .next()
            .ok_or_else(|| format!("missing value for {arg}"))?;
        match arg.as_str() {
            "--source" => source = value,
            "--port" => port = Some(value),
            "--bitrate" => {
                speed = match value.as_str() {
                    "250" => CanBusSpeed::Kbps250,
                    "500" => CanBusSpeed::Kbps500,
                    _ => return Err("bitrate must be 250 or 500".into()),
                }
            }
            "--udp-port" => udp_port = value.parse()?,
            "--dbc" => dbc = Some(std::path::PathBuf::from(value)),
            "--id" => {
                filter = Some(if let Some(hex) = value.strip_prefix("0x") {
                    u32::from_str_radix(hex, 16)?
                } else {
                    value.parse()?
                })
            }
            _ => return Err(format!("unknown option: {arg}").into()),
        }
    }
    let available = match source.as_str() {
        "serial" => cfg!(feature = "serial"),
        "loopback" => cfg!(feature = "loopback"),
        "udp" => cfg!(feature = "udp"),
        "simulated" => cfg!(feature = "simulated"),
        _ => false,
    };
    if !available {
        return Err(format!("source {source} is unavailable; enable its Cargo feature").into());
    }
    let source = match source.as_str() {
        "serial" => ConnectionSource::Serial(port.ok_or("--port is required for serial")?, speed),
        "loopback" => ConnectionSource::Loopback,
        "udp" => ConnectionSource::Udp(udp_port),
        "simulated" => ConnectionSource::Simulated(true, dbc.clone()),
        _ => return Err("unknown source".into()),
    };
    install_shutdown()?;
    let (tx, rx) = mpsc::channel();
    let config = CanThreadConfig {
        dbc_path: dbc,
        ..Default::default()
    };
    let mut worker = spawn_can_thread(config, tx)?;
    worker.command(CanThreadCommand::Connect(Some(source)))?;
    let mut session = Session::live(Time::now(), 30.0, 0.0);
    while !STOP.load(Ordering::Relaxed) {
        match rx.recv_timeout(Duration::from_millis(10)) {
            Ok(CanThreadEvent::Frame(frame)) => {
                if command == "watch" && filter.is_none_or(|id| id == frame.msg_id) {
                    println!(
                        "{} 0x{:X} {} {:02X?} {}",
                        frame.timestamp.label(),
                        frame.msg_id,
                        if frame.is_msg_id_extended {
                            "EXT"
                        } else {
                            "STD"
                        },
                        frame.raw_bytes,
                        frame
                            .decoded
                            .as_ref()
                            .map_or("undecoded", |d| d.name.as_str())
                    );
                }
                session.ingest_frame(frame);
            }
            Ok(CanThreadEvent::ConnectionSuccessful) => eprintln!("Connected"),
            Ok(CanThreadEvent::Disconnection) => eprintln!("Disconnected"),
            Ok(CanThreadEvent::ConnectionFailed(e)) | Ok(CanThreadEvent::Diagnostic(e)) => {
                eprintln!("{e}")
            }
            Ok(CanThreadEvent::SendFailed { msg_id, error, .. }) => {
                eprintln!("Send {msg_id:X}: {error}")
            }
            Ok(_) | Err(mpsc::RecvTimeoutError::Timeout) => {}
            Err(mpsc::RecvTimeoutError::Disconnected) => break,
        }
        session.evict();
    }
    worker.stop().map_err(|_| "CAN worker panicked")?;
    Ok(())
}
