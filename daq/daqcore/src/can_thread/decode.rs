use crate::{
    ParsedFrame, Time,
    frame::{CanFrame, FrameKind},
};
#[derive(Default)]
pub(super) struct FrameDecoder {
    parser: Option<can_decode::Parser>,
}
impl FrameDecoder {
    pub fn reload(&mut self, path: &std::path::Path) -> Result<(), String> {
        let parser = can_decode::Parser::from_dbc_file(path).map_err(|e| e.to_string())?;
        self.parser = Some(parser);
        Ok(())
    }
    pub fn decode(&self, frame: CanFrame, timestamp: Time) -> ParsedFrame {
        let decoded = if frame.kind == FrameKind::Data {
            self.parser
                .as_ref()
                .and_then(|p| p.decode_msg(frame.decode_id(), &frame.data))
        } else {
            None
        };
        ParsedFrame {
            kind: frame.kind,
            dlc: frame.dlc,
            timestamp,
            msg_id: frame.msg_id,
            is_msg_id_extended: frame.is_msg_id_extended,
            raw_bytes: frame.data,
            decoded,
        }
    }
}
#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn undecoded_frame_keeps_parse_timestamp_and_bytes() {
        let decoder = FrameDecoder::default();
        let now = Time::from_unix_millis(123);
        let frame = decoder.decode(CanFrame::data(3, true, vec![1, 2]).unwrap(), now);
        assert_eq!(frame.timestamp, now);
        assert_eq!(frame.raw_bytes, [1, 2]);
        assert!(frame.decoded.is_none());
    }
    #[test]
    fn failed_reload_keeps_working_decoder_and_fd_stays_raw() {
        let mut decoder = FrameDecoder::default();
        decoder
            .reload(
                &std::path::Path::new(env!("CARGO_MANIFEST_DIR")).join("tests/fixtures/test.dbc"),
            )
            .unwrap();
        assert!(
            decoder
                .reload(std::path::Path::new("/missing/dbc/for/test"))
                .is_err()
        );
        let frame = decoder.decode(
            CanFrame::data(3, false, vec![100, 0]).unwrap(),
            Time::from_unix_millis(42),
        );
        assert_eq!(
            frame.decoded.unwrap().signals["voltage"].value.physical,
            10.0
        );
        let frame = CanFrame {
            msg_id: 3,
            is_msg_id_extended: true,
            kind: FrameKind::Fd {
                bit_rate_switched: true,
            },
            dlc: 9,
            data: vec![1; 12],
        };
        let frame = decoder.decode(frame, Time::from_unix_millis(43));
        assert!(frame.decoded.is_none());
        assert_eq!(frame.raw_bytes.len(), 12);
        assert_eq!(frame.dlc, 9);
    }
}
