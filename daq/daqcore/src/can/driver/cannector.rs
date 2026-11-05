//! Receive-only native USB bridge for firmware/source/cannector.

use crate::{can, connection, frame, log_parse};
use rusb::UsbContext as _;
use std::{mem, time};

// The board advertises one vendor-class interface with a single bulk IN endpoint
// carrying CAN-to-USB records, per firmware/source/cannector/usb/usb_descriptors.h.
// The USB IDs are unregistered; that header notes they must be replaced with
// allocated IDs before distribution.
const VID: u16 = 0x5045;
const PID: u16 = 0x0271;
const CONFIGURATION: u8 = 1;
const INTERFACE: u8 = 0;
const BULK_IN: u8 = 0x81;

// Records mirror timestamped_frame_t in
// firmware/common/timestamped_frame/timestamped_frame.h: a u32 millisecond
// timestamp, a packed u32 identity, and eight padded payload bytes. The firmware
// sends 1-4 records per transfer, MAX_FRAMES_PER_PACKET in usb_device.h.
const RECORD_SIZE: usize = mem::size_of::<log_parse::parse::RawFrame>();
const MAX_RECORDS_PER_PACKET: usize = 4;
const PACKET_SIZE: usize = RECORD_SIZE * MAX_RECORDS_PER_PACKET;

// Bus IDs the firmware tags each frame with, from
// generators/configs/system/bus_configs.json. Frames from other buses never
// reach this board's three inputs.
const VCAN_BUS_ID: u8 = 0;
const MCAN_BUS_ID: u8 = 1;
const SCAN_BUS_ID: u8 = 3;

// A bulk read interrupted by a signal is retried rather than treated as a dead
// connection; bound the retries so a signal storm cannot spin here forever.
const INTERRUPTED_RETRIES: usize = 3;

pub struct CannectorDriver {
    handle: rusb::DeviceHandle<rusb::Context>,
    channel: connection::CannectorChannel,
}

impl CannectorDriver {
    pub fn new(channel: connection::CannectorChannel) -> can::driver::DriverResult<Self> {
        let context = rusb::Context::new()
            .map_err(|e| can::driver::DriverError::ConnectionFailed(e.to_string()))?;
        let devices = context
            .devices()
            .map_err(|e| can::driver::DriverError::ConnectionFailed(e.to_string()))?;
        let candidates: Vec<_> = devices
            .iter()
            .filter_map(|device| {
                let descriptor = device.device_descriptor().ok()?;
                (descriptor.vendor_id() == VID && descriptor.product_id() == PID)
                    .then_some((device, descriptor))
            })
            .collect();
        let (device, descriptor) = match candidates.as_slice() {
            [] => {
                return Err(can::driver::DriverError::ConnectionFailed(
                    "CANnector device 5045:0271 not found; connect its USB-C port".into(),
                ));
            }
            [device] => device,
            // The firmware exposes no serial-number string, so two boards cannot
            // be told apart; refuse instead of guessing.
            _ => {
                return Err(can::driver::DriverError::ConnectionFailed(
                    "multiple CANnectors connected; connect only the intended board".into(),
                ));
            }
        };

        // Do not assume any device sharing these unregistered IDs uses this protocol.
        let config = device
            .config_descriptor(0)
            .map_err(|e| can::driver::DriverError::ConnectionFailed(e.to_string()))?;
        let compatible = descriptor.class_code() == 0xff
            && descriptor.num_configurations() == 1
            && config.number() == CONFIGURATION
            && config.num_interfaces() == 1
            && config.interfaces().any(|interface| {
                interface.descriptors().any(|setting| {
                    setting.interface_number() == INTERFACE
                        && setting.setting_number() == 0
                        && setting.class_code() == 0xff
                        && setting.sub_class_code() == 0
                        && setting.protocol_code() == 0
                        && setting.num_endpoints() == 1
                        && setting.endpoint_descriptors().any(|endpoint| {
                            endpoint.address() == BULK_IN
                                && endpoint.transfer_type() == rusb::TransferType::Bulk
                                && usize::from(endpoint.max_packet_size()) == PACKET_SIZE
                        })
                })
            });
        if !compatible {
            return Err(can::driver::DriverError::Unsupported(
                "CANnector USB descriptors do not match the supported firmware".into(),
            ));
        }

        let handle = device
            .open()
            .map_err(|e| can::driver::DriverError::ConnectionFailed(e.to_string()))?;
        if handle
            .active_configuration()
            .map_err(|e| can::driver::DriverError::ConnectionFailed(e.to_string()))?
            != CONFIGURATION
        {
            handle
                .set_active_configuration(CONFIGURATION)
                .map_err(|e| can::driver::DriverError::ConnectionFailed(e.to_string()))?;
        }

        // No device reset, kernel-driver detachment, or vendor/CAN commands.
        handle
            .claim_interface(INTERFACE)
            .map_err(|e| can::driver::DriverError::ConnectionFailed(e.to_string()))?;

        // DeviceHandle releases the interface on drop, including on disconnect.
        Ok(Self { handle, channel })
    }

    // One endpoint-sized read preserves the firmware's record boundaries and
    // avoids waiting for another packet when all four records fill a packet.
    fn read_packet(&mut self, packet: &mut [u8; PACKET_SIZE]) -> can::driver::DriverResult<usize> {
        let mut interrupted = 0;
        loop {
            match self
                .handle
                .read_bulk(BULK_IN, packet, time::Duration::from_millis(10))
            {
                Ok(length) => return Ok(length),
                Err(rusb::Error::Timeout) => return Err(can::driver::DriverError::Timeout),
                Err(rusb::Error::Interrupted) if interrupted < INTERRUPTED_RETRIES => {
                    interrupted += 1;
                }
                // The endpoint stalls if the board resets mid-transfer; clear the
                // halt and let the next read continue instead of re-enumerating.
                Err(rusb::Error::Pipe) => {
                    return Err(match self.handle.clear_halt(BULK_IN) {
                        Ok(()) => can::driver::DriverError::Timeout,
                        Err(error) => can::driver::DriverError::Read(format!(
                            "CANnector endpoint halted: {error}"
                        )),
                    });
                }
                Err(error) => {
                    return Err(can::driver::DriverError::Read(format!(
                        "CANnector USB: {error}"
                    )));
                }
            }
        }
    }
}

impl can::driver::Driver for CannectorDriver {
    fn read_frames(&mut self) -> can::driver::DriverResult<Vec<frame::CanFrame>> {
        let mut packet = [0; PACKET_SIZE];
        let length = self.read_packet(&mut packet)?;
        let frames = decode_packet(&packet[..length], self.channel)?;
        if frames.is_empty() {
            Err(can::driver::DriverError::Timeout)
        } else {
            Ok(frames)
        }
    }

    fn write_frame(&mut self, _: frame::CanFrameTx) -> can::driver::DriverResult<()> {
        Err(can::driver::DriverError::Unsupported(
            "CANnector transmission is unsupported".into(),
        ))
    }

    fn can_transmit(&self) -> bool {
        false
    }

    fn bus_speed(&self) -> Option<connection::CanBusSpeed> {
        // Every input is fixed at 500 kbps by firmware that builds its bit rates
        // from generators/configs/system/bus_configs.json.
        Some(connection::CanBusSpeed::Kbps500)
    }
}

fn channel_bus_id(channel: connection::CannectorChannel) -> u8 {
    match channel {
        connection::CannectorChannel::Vcan => VCAN_BUS_ID,
        connection::CannectorChannel::Mcan => MCAN_BUS_ID,
        connection::CannectorChannel::Scan => SCAN_BUS_ID,
    }
}

fn decode_packet(
    packet: &[u8],
    channel: connection::CannectorChannel,
) -> can::driver::DriverResult<Vec<frame::CanFrame>> {
    if packet.len() > PACKET_SIZE || !packet.len().is_multiple_of(RECORD_SIZE) {
        return Err(can::driver::DriverError::Read(
            "CANnector packet must contain complete 16-byte records".into(),
        ));
    }
    let bus_id = channel_bus_id(channel);
    let mut frames = Vec::with_capacity(packet.len() / RECORD_SIZE);
    for bytes in packet.as_chunks::<RECORD_SIZE>().0 {
        let raw = bytemuck::try_pod_read_unaligned::<log_parse::parse::RawFrame>(bytes)
            .map_err(|e| can::driver::DriverError::Read(e.to_string()))?;
        if raw.bus_id() != bus_id {
            continue;
        }
        // Frames keep their physical input tag; the selected SuperDBC bus decides
        // how they decode, so bench traffic on VCAN may use TEST_CAN definitions.
        frames.push(
            frame::CanFrame::data(raw.can_identity(), raw.data.to_vec())
                .map_err(can::driver::DriverError::Read)?,
        );
    }
    Ok(frames)
}

#[cfg(test)]
mod tests {
    use super::*;
    use connection::CannectorChannel::{Mcan, Scan, Vcan};

    fn record(bus_id: u8, can_id: u32, payload: [u8; 8]) -> [u8; RECORD_SIZE] {
        let mut bytes = [0; RECORD_SIZE];
        bytes[..4].copy_from_slice(&123456_u32.to_le_bytes());
        let identity = can_id | (u32::from(bus_id) << log_parse::consts::BUS_ID_OFFSET);
        bytes[4..8].copy_from_slice(&identity.to_le_bytes());
        bytes[8..].copy_from_slice(&payload);
        bytes
    }

    #[test]
    fn decodes_standard_identity_and_keeps_padding() {
        let payload = [1, 2, 3, 4, 5, 6, 0, 0];
        let frames = decode_packet(&record(VCAN_BUS_ID, 0x123, payload), Vcan).unwrap();
        assert_eq!(frames.len(), 1);
        assert_eq!(frames[0].identity, 0x123);
        assert_eq!(frames[0].data, payload);
        assert_eq!(frames[0].dlc, 8);
        assert_eq!(frames[0].kind, frame::FrameKind::Data);
    }

    #[test]
    fn keeps_physical_channels_separate_and_decodes_extended_ids() {
        let packet = [
            record(VCAN_BUS_ID, 0x7ff, [1; 8]),
            record(MCAN_BUS_ID, 0x1abcde, [2; 8]),
            record(SCAN_BUS_ID, 0x2ab, [3; 8]),
            record(VCAN_BUS_ID, 0x4, [4; 8]),
        ]
        .concat();

        let vcan = decode_packet(&packet, Vcan).unwrap();
        assert_eq!(
            vcan.iter().map(|f| f.identity).collect::<Vec<_>>(),
            [0x7ff, 0x4]
        );

        let mcan = decode_packet(&packet, Mcan).unwrap();
        assert_eq!(mcan.len(), 1);
        assert_eq!(mcan[0].identity, 0x1abcde);
        assert_eq!(mcan[0].data, [2; 8]);

        let scan = decode_packet(&packet, Scan).unwrap();
        assert_eq!(scan.len(), 1);
        assert_eq!(scan[0].identity, 0x2ab);
    }

    #[test]
    fn ignores_buses_this_board_has_no_input_for() {
        // CCAN (2) and TEST_CAN (4) never reach the CANnector's three inputs.
        for bus_id in [2, 4] {
            let packet = record(bus_id, 0x10, [0; 8]);
            for channel in connection::CannectorChannel::options() {
                assert!(decode_packet(&packet, channel).unwrap().is_empty());
            }
        }
    }

    #[test]
    fn accepts_one_to_four_records_and_empty_reads() {
        for count in 0..=MAX_RECORDS_PER_PACKET {
            let packet = record(VCAN_BUS_ID, 0x3, [0; 8]).repeat(count);
            assert_eq!(decode_packet(&packet, Vcan).unwrap().len(), count);
        }
    }

    #[test]
    fn rejects_partial_and_oversized_transfers() {
        for length in [1, 15, 17, 31, 63, 65, 80] {
            assert!(matches!(
                decode_packet(&vec![0; length], Vcan),
                Err(can::driver::DriverError::Read(_))
            ));
        }
    }

    #[test]
    fn preserves_the_full_extended_id_range() {
        let frames = decode_packet(&record(MCAN_BUS_ID, 0x1fffffff, [0; 8]), Mcan).unwrap();
        assert_eq!(frames[0].identity, 0x1fffffff);
    }
}
