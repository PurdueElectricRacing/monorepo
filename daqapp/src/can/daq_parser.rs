use crate::daq_log_parse::consts::{IS_EID_MASK, identity_with_bus_id};
use crate::daq_log_parse::parse::RawFrame;
use crate::messages::BusName;

use chrono::{Datelike, Timelike};
use std::fs::{File, create_dir_all};
use std::io::Write;
use std::path::PathBuf;
use std::time::Instant;

pub const LOG_FILE_ROTATE_MS: u128 = 60000;
pub const DEFAULT_FLUSH_MS: u128 = 1000;

pub fn byte_to_bcd_format(val: u8) -> u8 {
    ((val / 10) << 4) | (val % 10)
}

struct OpenLogFile {
    file: File,
    current_file_path: PathBuf,
}

pub struct DaqLogger {
    open_file: Option<OpenLogFile>,
    folder_path: PathBuf,
    buffer: Vec<RawFrame>,
    file_created_at: Instant,
    start_time: Instant,
    last_flush: Instant,
    buffer_capacity: usize,
}

impl DaqLogger {
    pub fn new(folder_path: std::path::PathBuf) -> Self {
        if let Err(e) = create_dir_all(&folder_path) {
            log::error!(
                "Failed to create directory for logs: {:?}: {}",
                folder_path,
                e
            );
        }

        Self {
            open_file: None,
            folder_path: folder_path,
            buffer: Vec::with_capacity(10000),
            file_created_at: Instant::now(),
            start_time: Instant::now(),
            last_flush: Instant::now(),
            buffer_capacity: 5000,
        }
    }

    pub fn reset_start_time(&mut self) {
        self.start_time = Instant::now();
    }

    pub fn update_folder(&mut self, new_folder: std::path::PathBuf) {
        self.flush();
        self.open_file = None;
        self.folder_path = new_folder;
        if let Err(e) = create_dir_all(&self.folder_path) {
            log::error!(
                "Failed to create directory for logs: {:?}: {}",
                self.folder_path,
                e
            );
        }
    }

    pub fn log_can2_frame(&mut self, frame: &slcan::Can2Frame, bus_name: BusName) {
        let (id, data) = match frame.id() {
            slcan::Id::Standard(sid) => {
                let id = sid.as_raw() as u32;
                (id, frame.data().unwrap_or(&[]))
            }
            slcan::Id::Extended(eid) => {
                let id = eid.as_raw() | IS_EID_MASK;
                (id, frame.data().unwrap_or(&[]))
            }
        };

        let frame_identity = identity_with_bus_id(id, bus_name);

        let mut data_array = [0u8; 8];
        let len = data.len().min(8);
        data_array[..len].copy_from_slice(&data[..len]);

        let ticks_ms = self.start_time.elapsed().as_millis() as u32;

        let raw_frame = RawFrame {
            ticks_ms: ticks_ms,
            identity: frame_identity,
            data: data_array,
        };

        self.add_frame(raw_frame);
    }

    fn add_frame(&mut self, frame: RawFrame) {
        self.buffer.push(frame);

        //Flush every 1 second
        if self.buffer.len() >= self.buffer_capacity
            || self.last_flush.elapsed().as_millis() >= DEFAULT_FLUSH_MS
        {
            self.flush();
        }
    }

    pub fn flush(&mut self) {
        if self.buffer.is_empty() {
            return;
        }

        // Create new file if time of creation has exceed threshold, or if the
        // current log file/folder has been deleted out from under us (e.g. someone
        // cleared the logs folder while daqapp was still running).
        let rotated_out = self.open_file.is_some()
            && self.file_created_at.elapsed().as_millis() >= LOG_FILE_ROTATE_MS;
        let deleted_out = self
            .open_file
            .as_ref()
            .is_some_and(|f| !f.current_file_path.exists());

        if rotated_out || deleted_out {
            self.open_file = None;
        }

        if self.open_file.is_none() {
            let now = chrono::Local::now();
            self.file_created_at = Instant::now();

            let year_bcd = byte_to_bcd_format((now.year() % 100) as u8);
            let month_bcd = byte_to_bcd_format(now.month() as u8);
            let day_bcd = byte_to_bcd_format(now.day() as u8);
            let hour_bcd = byte_to_bcd_format(now.hour() as u8);
            let min_bcd = byte_to_bcd_format(now.minute() as u8);
            let sec_bcd = byte_to_bcd_format(now.second() as u8);

            let filename = format!(
                "log-20{:02x}-{:02x}-{:02x}--{:02x}-{:02x}-{:02x}.log",
                year_bcd, month_bcd, day_bcd, hour_bcd, min_bcd, sec_bcd
            );

            let file_path = self.folder_path.join(filename);
            let created = File::create(&file_path).or_else(|e| {
                log::warn!(
                    "Failed to create log file {:?}: {}; recreating log folder",
                    file_path,
                    e
                );
                if let Err(e) = create_dir_all(&self.folder_path) {
                    log::error!(
                        "Failed to recreate directory for logs: {:?}: {}",
                        self.folder_path,
                        e
                    );
                }
                File::create(&file_path)
            });

            match created {
                Ok(file) => {
                    self.open_file = Some(OpenLogFile {
                        file,
                        current_file_path: file_path,
                    });
                }
                Err(e) => {
                    log::error!("Failed to create log file {:?}: {}", file_path, e);
                    self.buffer.clear();
                    self.last_flush = Instant::now();
                    return;
                }
            }
        }

        if let Some(ref mut open_file) = self.open_file {
            if let Err(e) = open_file.file.write_all(bytemuck::cast_slice(&self.buffer)) {
                log::error!("Failed to write to log file: {}", e);
            }

            if let Err(e) = open_file.file.flush() {
                log::error!("Failed to flush log file: {}", e);
            }
        }

        self.buffer.clear();
        self.last_flush = Instant::now();
    }
}

impl Drop for DaqLogger {
    fn drop(&mut self) {
        self.flush();
        if let Some(open_file) = self.open_file.take() {
            let _ = open_file.file.sync_all();
        }
    }
}
