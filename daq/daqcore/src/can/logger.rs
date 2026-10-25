use crate::{frame, log_parse};
use chrono::Datelike as _;
use chrono::Timelike as _;
use std::{fs, io, path, time};

pub const LOG_FILE_ROTATE_MS: u128 = 60000;
pub const DEFAULT_FLUSH_MS: u128 = 1000;

pub fn byte_to_bcd_format(val: u8) -> u8 {
    ((val / 10) << 4) | (val % 10)
}

struct OpenLogFile {
    file: fs::File,
    current_file_path: path::PathBuf,
}

pub struct DaqLogger {
    open_file: Option<OpenLogFile>,
    folder_path: path::PathBuf,
    buffer: Vec<log_parse::parse::RawFrame>,
    file_created_at: time::Instant,
    start_time: time::Instant,
    last_flush: time::Instant,
    buffer_capacity: usize,
}

impl DaqLogger {
    pub fn new(folder_path: path::PathBuf) -> Self {
        if let Err(e) = fs::create_dir_all(&folder_path) {
            log::error!(
                "Failed to create directory for logs: {:?}: {}",
                folder_path,
                e
            );
        }

        Self {
            open_file: None,
            folder_path,
            buffer: Vec::with_capacity(10000),
            file_created_at: time::Instant::now(),
            start_time: time::Instant::now(),
            last_flush: time::Instant::now(),
            buffer_capacity: 5000,
        }
    }

    pub fn reset_start_time(&mut self) {
        self.start_time = time::Instant::now();
    }

    pub fn update_folder(&mut self, new_folder: path::PathBuf) {
        self.flush();
        self.open_file = None;
        self.folder_path = new_folder;
        if let Err(e) = fs::create_dir_all(&self.folder_path) {
            log::error!(
                "Failed to create directory for logs: {:?}: {}",
                self.folder_path,
                e
            );
        }
    }

    pub fn log_frame(&mut self, frame: &frame::CanFrame, bus_id: u8) -> Result<(), String> {
        if matches!(frame.kind, frame::FrameKind::Fd { .. }) {
            return Ok(());
        }

        if frame.data.len() > 8 {
            return Err("classic CAN log payload exceeds 8 bytes".into());
        }

        // Convert vec to array, fill with 0s
        let data = std::array::from_fn(|i| frame.data.get(i).copied().unwrap_or(0));

        let raw = log_parse::parse::RawFrame::new(
            self.start_time.elapsed().as_millis() as u32,
            frame.identity,
            bus_id,
            data,
        )?;

        self.add_frame(raw);

        Ok(())
    }

    fn add_frame(&mut self, frame: log_parse::parse::RawFrame) {
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
            self.file_created_at = time::Instant::now();

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
            let created = fs::File::create(&file_path).or_else(|e| {
                log::warn!(
                    "Failed to create log file {:?}: {}; recreating log folder",
                    file_path,
                    e
                );
                if let Err(e) = fs::create_dir_all(&self.folder_path) {
                    log::error!(
                        "Failed to recreate directory for logs: {:?}: {}",
                        self.folder_path,
                        e
                    );
                }
                fs::File::create(&file_path)
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
                    self.last_flush = time::Instant::now();
                    return;
                }
            }
        }

        if let Some(ref mut open_file) = self.open_file {
            if let Err(e) =
                io::Write::write_all(&mut open_file.file, bytemuck::cast_slice(&self.buffer))
            {
                log::error!("Failed to write to log file: {}", e);
            }

            if let Err(e) = io::Write::flush(&mut open_file.file) {
                log::error!("Failed to flush log file: {}", e);
            }
        }

        self.buffer.clear();
        self.last_flush = time::Instant::now();
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
