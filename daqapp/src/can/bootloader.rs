//! CAN-side state machine for one validated firmware package.
//!
//! The target acknowledges erase and CRC boundaries, but intentionally does not
//! acknowledge every data word. This updater therefore streams ordered words,
//! waits at the three command boundaries, and treats the absence of a response
//! after `JUMP` as the expected successful application hand-off.

use crate::{bootloader_protocol::FirmwarePackage, messages};

// Values mirror BLCmd_t/BLStatus_t in firmware/common/bootloader. Keeping the
// wire constants here avoids coupling the host updater to generated C headers.
const PROTOCOL_VERSION: u32 = 1;
const START: u8 = 0x01;
const CRC: u8 = 0x03;
const JUMP: u8 = 0x04;
const READY: u8 = 0x00;
const ACK: u8 = 0x01;
const CRC_ERROR: u8 = 0x03;

// Erase can take longer than an ordinary response, while command retries are
// bounded so a disconnected node cannot leave the CAN thread busy forever.
const BOOT_TIMEOUT: std::time::Duration = std::time::Duration::from_secs(5);
const COMMAND_TIMEOUT: std::time::Duration = std::time::Duration::from_secs(2);
// A successful JUMP resets the node and produces no response; this is the
// grace period before advancing to the next board in a multi-board package.
const JUMP_DELAY: std::time::Duration = std::time::Duration::from_millis(700);
const MAX_RETRIES: u8 = 3;

/// A validated classic-CAN frame ready for the CAN driver.
#[derive(Clone, Debug)]
pub struct OutboundFrame {
    /// Standard 11-bit ID; validation happens again at the driver boundary.
    pub id: u32,
    /// Five-byte command or six-byte indexed data payload.
    pub data: Vec<u8>,
}

#[derive(Debug)]
enum Stage {
    /// Initial START is requesting an already-running application to reset.
    WaitReady,
    /// Bootloader is erasing staging after the second START.
    WaitStartAck,
    /// Ordered data words are streamed without per-word responses.
    SendingData,
    /// Target is calculating CRC, copying, and committing metadata.
    WaitCrcAck,
    /// JUMP was sent; target should now be running the application.
    WaitJump,
    /// Terminal success, cancellation, or error.
    Finished,
}

/// Progress-aware updater for all images in one validated package.
pub struct FirmwareUpdater {
    package: FirmwarePackage,
    /// Index of the board currently being updated.
    board_index: usize,
    /// Number of image bytes already converted into outbound data frames.
    byte_offset: usize,
    stage: Stage,
    /// A handshake frame that should be sent before normal tick processing.
    pending_frame: Option<OutboundFrame>,
    deadline: std::time::Instant,
    retries: u8,
}

/// Work and UI information returned to the CAN thread on each tick.
pub struct TickResult {
    /// At most one frame is emitted per call; the thread may batch calls.
    pub frame: Option<OutboundFrame>,
    /// Optional phase transition/progress event for the UI.
    pub progress: Option<messages::FirmwareProgress>,
}

impl FirmwareUpdater {
    /// Start the first board by sending the application-side START request.
    ///
    /// The initial frame is queued immediately. If the board is already in its
    /// bootloader (for example, because no valid app exists), its ACK is also a
    /// valid transition into data streaming; otherwise READY causes the
    /// updater to send the erase START a second time.
    pub fn new(package: FirmwarePackage) -> (Self, messages::FirmwareProgress) {
        let now = std::time::Instant::now();
        let mut updater = Self {
            package,
            board_index: 0,
            byte_offset: 0,
            stage: Stage::WaitReady,
            pending_frame: None,
            deadline: now + BOOT_TIMEOUT,
            retries: 0,
        };
        let first = updater.current_image();
        updater.pending_frame = Some(Self::start_frame(first));
        let progress = updater.progress("requesting bootloader", None);
        (updater, progress)
    }

    fn current_image(&self) -> &crate::bootloader_protocol::FirmwareImage {
        &self.package.images[self.board_index]
    }

    /// START carries the exact padded image length expected by the target.
    fn start_frame(image: &crate::bootloader_protocol::FirmwareImage) -> OutboundFrame {
        OutboundFrame {
            id: image.command_id,
            data: command(START, image.bytes.len() as u32),
        }
    }

    /// CRC carries the host CRC already verified while loading the package.
    fn crc_frame(image: &crate::bootloader_protocol::FirmwareImage) -> OutboundFrame {
        OutboundFrame {
            id: image.command_id,
            data: command(CRC, image.crc32),
        }
    }

    /// JUMP has no meaningful argument and normally has no response.
    fn jump_frame(image: &crate::bootloader_protocol::FirmwareImage) -> OutboundFrame {
        OutboundFrame {
            id: image.command_id,
            data: command(JUMP, 0),
        }
    }

    /// Build a snapshot for the UI without exposing the internal stage enum.
    fn progress(
        &self,
        phase: impl Into<String>,
        error: Option<String>,
    ) -> messages::FirmwareProgress {
        let image = self.current_image();
        messages::FirmwareProgress {
            board: image.name.clone(),
            board_index: self.board_index,
            board_count: self.package.images.len(),
            phase: phase.into(),
            sent_bytes: self.byte_offset,
            total_bytes: image.bytes.len(),
            error,
        }
    }

    /// Make failure terminal and emit exactly one error snapshot.
    fn fail(&mut self, error: String) -> messages::FirmwareProgress {
        self.stage = Stage::Finished;
        self.progress("failed", Some(error))
    }

    /// Whether the updater will produce any more frames or progress events.
    pub fn is_finished(&self) -> bool {
        matches!(self.stage, Stage::Finished)
    }

    /// Stop locally. The target is intentionally not reset here; the UI should
    /// treat a mid-transfer cancellation as a node that may still be in the
    /// bootloader and should be updated again before vehicle use.
    pub fn cancel(&mut self) -> messages::FirmwareProgress {
        self.fail("cancelled by user".to_string())
    }

    /// Advance timers/state and return the next frame, if one is due.
    ///
    /// Data frames are generated one word at a time and have no individual
    /// deadline. The CAN thread batches these calls to keep a serial adapter
    /// responsive while command stages remain bounded by `deadline`.
    pub fn tick(&mut self, now: std::time::Instant) -> TickResult {
        if self.is_finished() {
            return TickResult {
                frame: None,
                progress: None,
            };
        }

        if let Some(frame) = self.pending_frame.take() {
            // Handshake frames take priority over data generation. This keeps
            // a retry/phase transition from being delayed by a large image.
            return TickResult {
                frame: Some(frame),
                progress: None,
            };
        }

        if now >= self.deadline {
            let (frame, phase) = match self.stage {
                Stage::WaitReady => (
                    Some(Self::start_frame(self.current_image())),
                    "retrying bootloader request",
                ),
                Stage::WaitStartAck => (
                    Some(Self::start_frame(self.current_image())),
                    "retrying erase",
                ),
                Stage::WaitCrcAck => (Some(Self::crc_frame(self.current_image())), "retrying CRC"),
                Stage::WaitJump => {
                    // No response is expected after JUMP. Once the grace
                    // period expires, either start the next board or finish.
                    if self.board_index + 1 < self.package.images.len() {
                        self.board_index += 1;
                        self.byte_offset = 0;
                        self.stage = Stage::WaitReady;
                        self.retries = 0;
                        self.deadline = now + BOOT_TIMEOUT;
                        let frame = Self::start_frame(self.current_image());
                        return TickResult {
                            frame: Some(frame),
                            progress: Some(self.progress("requesting bootloader", None)),
                        };
                    }
                    self.stage = Stage::Finished;
                    return TickResult {
                        frame: None,
                        progress: Some(self.progress("complete", None)),
                    };
                }
                Stage::SendingData | Stage::Finished => (None, ""),
            };

            if let Some(frame) = frame {
                if self.retries >= MAX_RETRIES {
                    let progress =
                        self.fail(format!("no response from {}", self.current_image().name));
                    return TickResult {
                        frame: None,
                        progress: Some(progress),
                    };
                }
                self.retries += 1;
                self.deadline = now
                    + if matches!(self.stage, Stage::WaitReady) {
                        BOOT_TIMEOUT
                    } else {
                        COMMAND_TIMEOUT
                    };
                return TickResult {
                    frame: Some(frame),
                    progress: Some(self.progress(phase, None)),
                };
            }
        }

        if matches!(self.stage, Stage::SendingData) {
            if self.byte_offset >= self.current_image().bytes.len() {
                // The target has already accepted every word; CRC is the next
                // response-bearing boundary and commits only on success.
                let crc_frame = Self::crc_frame(self.current_image());
                self.stage = Stage::WaitCrcAck;
                self.deadline = now + COMMAND_TIMEOUT;
                self.retries = 0;
                return TickResult {
                    frame: Some(crc_frame),
                    progress: Some(self.progress("verifying CRC", None)),
                };
            }

            // FirmwarePackage::load guarantees a four-byte boundary, so each
            // slice below is exactly one target data word.
            let index = (self.byte_offset / 4) as u16;
            let frame = {
                let image = self.current_image();
                let word = &image.bytes[self.byte_offset..self.byte_offset + 4];
                OutboundFrame {
                    id: image.data_id,
                    data: vec![
                        (index & 0xFF) as u8,
                        (index >> 8) as u8,
                        word[0],
                        word[1],
                        word[2],
                        word[3],
                    ],
                }
            };
            self.byte_offset += 4;
            let image_len = self.current_image().bytes.len();
            let progress = if self.byte_offset % (4 * 32) == 0 || self.byte_offset == image_len {
                Some(self.progress("uploading", None))
            } else {
                None
            };
            return TickResult {
                frame: Some(frame),
                progress,
            };
        }

        TickResult {
            frame: None,
            progress: None,
        }
    }

    /// Consume a response from the current board, if its ID and payload are valid.
    ///
    /// Responses for other nodes are ignored because the CAN bus can contain
    /// unrelated traffic. A target CRC error or protocol error is terminal for
    /// this package; the host does not attempt to guess which words were lost.
    pub fn on_response(
        &mut self,
        id: u32,
        data: &[u8],
        now: std::time::Instant,
    ) -> Option<messages::FirmwareProgress> {
        if self.is_finished() || id != self.current_image().response_id || data.len() < 5 {
            return None;
        }
        let status = data[0];
        // Response detail uses the same little-endian uint32 representation as
        // command arguments and the firmware metadata CRC.
        let detail = u32::from_le_bytes([data[1], data[2], data[3], data[4]]);
        let image_name = self.current_image().name.clone();
        let image_size = self.current_image().bytes.len() as u32;
        let image_crc = self.current_image().crc32;

        if status == CRC_ERROR {
            return Some(self.fail(format!("CRC rejected by {image_name}")));
        }
        if status != READY && status != ACK {
            return Some(self.fail(format!("bootloader error 0x{status:02X} from {image_name}")));
        }

        let invalid_detail = match self.stage {
            Stage::WaitReady if status == READY => detail != PROTOCOL_VERSION,
            Stage::WaitReady | Stage::WaitStartAck if status == ACK => detail != image_size,
            Stage::WaitCrcAck if status == ACK => detail != image_crc,
            _ => false,
        };
        if invalid_detail {
            return Some(self.fail(format!("invalid response from {image_name}")));
        }

        match self.stage {
            Stage::WaitReady if status == READY => {
                // A running application has reset into the bootloader. Send a
                // fresh START now that READY proves the recovery image is up.
                let start_frame = Self::start_frame(self.current_image());
                self.stage = Stage::WaitStartAck;
                self.deadline = now + COMMAND_TIMEOUT;
                self.retries = 0;
                self.pending_frame = Some(start_frame);
                Some(self.progress("erasing staging flash", None))
            }
            Stage::WaitReady if status == ACK => {
                // A node already in the bootloader accepted the initial START,
                // so no second erase request is needed.
                self.stage = Stage::SendingData;
                self.retries = 0;
                Some(self.progress("uploading", None))
            }
            Stage::WaitStartAck if status == ACK => {
                self.stage = Stage::SendingData;
                self.retries = 0;
                Some(self.progress("uploading", None))
            }
            Stage::WaitCrcAck if status == ACK => {
                // ACK detail must echo the validated CRC. Only then is JUMP
                // allowed to hand the node back to application code.
                let jump_frame = Self::jump_frame(self.current_image());
                self.stage = Stage::WaitJump;
                self.deadline = now + JUMP_DELAY;
                self.pending_frame = Some(jump_frame);
                Some(self.progress("restarting application", None))
            }
            _ => None,
        }
    }
}

/// Encode a five-byte command frame payload in the target's wire order.
fn command(command: u8, argument: u32) -> Vec<u8> {
    let bytes = argument.to_le_bytes();
    vec![command, bytes[0], bytes[1], bytes[2], bytes[3]]
}
