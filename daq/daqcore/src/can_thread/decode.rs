use crate::{
    ParsedFrame, Time,
    frame::{CanFrame, FrameKind},
};
#[derive(Default)]
pub struct FrameDecoder {
    parser: Option<can_decode::Parser>,
}

impl FrameDecoder {
    pub fn reload(&mut self, path: &std::path::Path) -> Result<(), String> {
        let parser = can_decode::Parser::from_dbc_file(path).map_err(|error| error.to_string())?;
        self.parser = Some(parser);
        Ok(())
    }

    pub fn decode(&self, frame: CanFrame, timestamp: Time) -> ParsedFrame {
        let decoded = if frame.kind == FrameKind::Data {
            self.parser
                .as_ref()
                .and_then(|p| p.decode_msg(frame.identity.dbc_id(), &frame.data))
        } else {
            None
        };
        ParsedFrame {
            kind: frame.kind,
            dlc: frame.dlc,
            timestamp,
            identity: frame.identity,
            raw_bytes: frame.data,
            decoded,
        }
    }
}
