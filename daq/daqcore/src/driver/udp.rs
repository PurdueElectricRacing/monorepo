use crate::{driver, frame};
use std::net;

const MAX_UDP_PACKET_BYTES: usize = 65_536;

pub struct UdpDriver {
    socket: net::UdpSocket,
}

impl UdpDriver {
    pub fn new(port: u16) -> driver::DriverResult<Self> {
        let socket = net::UdpSocket::bind(("0.0.0.0", port))
            .map_err(|error| driver::DriverError::ConnectionFailed(error.to_string()))?;
        socket
            .set_read_timeout(Some(driver::READ_TIMEOUT))
            .map_err(|error| driver::DriverError::ConnectionFailed(error.to_string()))?;
        Ok(Self { socket })
    }
}

impl driver::Driver for UdpDriver {
    fn read_frames(&mut self) -> driver::DriverResult<Vec<frame::CanFrame>> {
        let mut buf = [0; MAX_UDP_PACKET_BYTES];
        let len = self.socket.recv(&mut buf).map_err(driver::io_read)?;
        driver::parse_daq_buffer(&buf[..len])
    }

    fn write_frame(&mut self, _: frame::CanFrameTx) -> driver::DriverResult<()> {
        Err(driver::DriverError::Unsupported(
            "UDP transmission is unsupported".into(),
        ))
    }
}
