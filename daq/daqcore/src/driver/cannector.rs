use crate::{can, driver, frame};

const CLASSIC_CAN_MAX_BYTES: usize = 8;

#[repr(C)]
#[derive(bytemuck::Pod, bytemuck::Zeroable, Copy, Clone, Debug, PartialEq, Eq)]
// Matches can_tx_frame_t in firmware/source/cannector/usb_device.h
struct CannectorTxFrame {
    can_id: u32,
    bus_id: u8,
    dlc: u8,
    is_extended_id: u8,
    reserved: u8,
    data: [u8; CLASSIC_CAN_MAX_BYTES],
}

impl CannectorTxFrame {
    fn from_tx_frame(tx_frame: frame::CanFrameTx) -> driver::DriverResult<Self> {
        let frame = tx_frame.frame;
        match frame.kind {
            frame::FrameKind::Data => {}
            frame::FrameKind::Remote => {
                return Err(driver::DriverError::Unsupported(
                    "CANnector remote frame transmission is unsupported".into(),
                ));
            }
            frame::FrameKind::Fd { .. } => {
                return Err(driver::DriverError::Unsupported(
                    "CANnector CAN FD transmission is unsupported".into(),
                ));
            }
        }

        let max_id = if tx_frame.is_extended_id {
            can::EXTENDED_ID_MASK
        } else {
            can::STANDARD_ID_MASK
        };
        if frame.identity > max_id {
            return Err(driver::DriverError::Write("invalid ID".into()));
        }
        if frame.data.len() > CLASSIC_CAN_MAX_BYTES || usize::from(frame.dlc) != frame.data.len() {
            return Err(driver::DriverError::Write("invalid payload".into()));
        }

        let mut data = [0u8; CLASSIC_CAN_MAX_BYTES];
        data[..frame.data.len()].copy_from_slice(&frame.data);

        let bus_id = frame.bus_id.unwrap_or_else(|| {
            log::warn!(
                "CANnector TX frame (ID 0x{:X}) has no bus_id; defaulting to bus 0 (VCAN)",
                frame.identity
            );
            0
        });

        Ok(Self {
            can_id: frame.identity,
            bus_id,
            dlc: frame.dlc,
            is_extended_id: u8::from(tx_frame.is_extended_id),
            reserved: 0,
            data,
        })
    }
}

pub struct CannectorDriver {
    handle: rusb::DeviceHandle<rusb::GlobalContext>,
}

impl CannectorDriver {
    pub const VID: u16 = 0x5045;
    pub const PID: u16 = 0x0271;
    const CONFIGURATION: u8 = 1;
    const INTERFACE: u8 = 0;
    const BULK_IN_ENDPOINT: u8 = 0x81;
    const BULK_OUT_ENDPOINT: u8 = 0x01;
    const PACKET_SIZE: usize = 64; // 4 timestamped frames

    pub fn new() -> driver::DriverResult<Self> {
        let handle = rusb::open_device_with_vid_pid(Self::VID, Self::PID).ok_or_else(|| {
            driver::DriverError::ConnectionFailed(
                "CANnector not found or USB access denied".into(),
            )
        })?;
        let _ = handle.set_auto_detach_kernel_driver(true);
        handle
            .set_active_configuration(Self::CONFIGURATION)
            .and_then(|()| handle.claim_interface(Self::INTERFACE))
            .map_err(|error| driver::DriverError::ConnectionFailed(error.to_string()))?;
        Ok(Self { handle })
    }
}

impl driver::Driver for CannectorDriver {
    fn read_frames(&mut self) -> driver::DriverResult<Vec<frame::CanFrame>> {
        // Read one packet
        // Avoids timeout after a full 64-byte packet
        let mut buffer = [0; Self::PACKET_SIZE];
        let len = self
            .handle
            .read_bulk(Self::BULK_IN_ENDPOINT, &mut buffer, driver::READ_TIMEOUT)
            .map_err(|error| match error {
                rusb::Error::Timeout => driver::DriverError::Timeout,
                other => driver::DriverError::Read(other.to_string()),
            })?;
        driver::parse_daq_buffer(&buffer[..len])
    }

    fn write_frame(&mut self, tx_frame: frame::CanFrameTx) -> driver::DriverResult<()> {
        let raw = CannectorTxFrame::from_tx_frame(tx_frame)?;
        let bytes = bytemuck::bytes_of(&raw);
        let written = self
            .handle
            .write_bulk(Self::BULK_OUT_ENDPOINT, bytes, driver::WRITE_TIMEOUT)
            .map_err(|error| driver::DriverError::Write(error.to_string()))?;
        if written != bytes.len() {
            return Err(driver::DriverError::Write(
                "incomplete USB bulk write".into(),
            ));
        }
        Ok(())
    }
}
