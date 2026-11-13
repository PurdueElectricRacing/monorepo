use crate::{connection, driver, frame};

const SERIAL_BAUD_RATE: u32 = 115_200;

pub struct SerialDriver {
    socket: slcan::sync::CanSocket<Box<dyn serialport::SerialPort>>,
    speed: connection::CanBusSpeed,
}

impl SerialDriver {
    pub fn new(path: &str, speed: connection::CanBusSpeed) -> driver::DriverResult<Self> {
        let port = serialport::new(path, SERIAL_BAUD_RATE)
            .timeout(driver::READ_TIMEOUT)
            .open()
            .map_err(|error| driver::DriverError::ConnectionFailed(error.to_string()))?;
        let _ = port.clear(serialport::ClearBuffer::All);
        let mut socket = slcan::sync::CanSocket::new(port);
        socket
            .set_operating_mode(slcan::OperatingMode::Normal)
            .map_err(|error| driver::DriverError::ConnectionFailed(error.to_string()))?;
        let bitrate = match speed {
            connection::CanBusSpeed::Kbps250 => slcan::NominalBitRate::Rate250Kbit,
            connection::CanBusSpeed::Kbps500 => slcan::NominalBitRate::Rate500Kbit,
        };
        socket
            .open(bitrate)
            .map_err(|error| driver::DriverError::ConnectionFailed(error.to_string()))?;
        Ok(Self { socket, speed })
    }
}

fn to_wire_id(raw_id: u32, is_extended_id: bool) -> Option<slcan::Id> {
    if is_extended_id {
        slcan::ExtendedId::new(raw_id).map(slcan::Id::Extended)
    } else {
        u16::try_from(raw_id)
            .ok()
            .and_then(slcan::StandardId::new)
            .map(slcan::Id::Standard)
    }
}

fn identity(id: slcan::Id) -> frame::CanIdentity {
    match id {
        slcan::Id::Standard(id) => u32::from(id.as_raw()),
        slcan::Id::Extended(id) => id.as_raw(),
    }
}

fn from_wire(frame: slcan::CanFrame) -> driver::DriverResult<frame::CanFrame> {
    let frame = match frame {
        slcan::CanFrame::Can2(wire_frame) => {
            let identity = identity(wire_frame.id());
            let kind = if wire_frame.is_remote() {
                frame::FrameKind::Remote
            } else {
                frame::FrameKind::Data
            };

            frame::CanFrame {
                bus_id: None,
                identity,
                kind,
                dlc: wire_frame.dlc() as u8,
                data: wire_frame.data().unwrap_or(&[]).to_vec(),
            }
        }
        slcan::CanFrame::CanFd(wire_frame) => {
            let identity = identity(wire_frame.id());
            frame::CanFrame {
                bus_id: None,
                identity,
                kind: frame::FrameKind::Fd {
                    bit_rate_switched: wire_frame.is_bit_rate_switched(),
                },
                dlc: wire_frame.dlc() as u8,
                data: wire_frame.data().to_vec(),
            }
        }
    };

    Ok(frame)
}

impl driver::Driver for SerialDriver {
    fn read_frames(&mut self) -> driver::DriverResult<Vec<frame::CanFrame>> {
        self.socket
            .read()
            .map_err(|error| match error {
                slcan::ReadError::Io(io_error) => driver::io_read(io_error),
                other => driver::DriverError::Read(format!("{other:?}")),
            })
            .and_then(from_wire)
            .map(|frame| vec![frame])
    }

    fn write_frame(&mut self, tx_frame: frame::CanFrameTx) -> driver::DriverResult<()> {
        let id = to_wire_id(tx_frame.frame.identity, tx_frame.is_extended_id)
            .ok_or_else(|| driver::DriverError::Write("invalid ID".into()))?;
        let frame = tx_frame.frame;
        let wire: slcan::CanFrame = match frame.kind {
            frame::FrameKind::Data => slcan::Can2Frame::new_data(id, &frame.data).map(Into::into),
            frame::FrameKind::Remote => {
                slcan::Can2Frame::new_remote(id, usize::from(frame.dlc)).map(Into::into)
            }
            frame::FrameKind::Fd { bit_rate_switched } => slcan::CanFdFrame::new(id, &frame.data)
                .map(|fd_frame| fd_frame.with_bit_rate_switched(bit_rate_switched).into()),
        }
        .ok_or_else(|| driver::DriverError::Write("invalid payload".into()))?;
        self.socket
            .send(wire)
            .map_err(|error| driver::DriverError::Write(error.to_string()))
    }

    fn bus_speed(&self) -> Option<connection::CanBusSpeed> {
        Some(self.speed)
    }

    fn close(&mut self) -> driver::DriverResult<()> {
        self.socket
            .close()
            .map_err(|error| driver::DriverError::Write(error.to_string()))
    }
}
