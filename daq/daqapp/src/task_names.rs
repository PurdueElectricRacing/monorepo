//! Reassemble diagnostics task names without keeping their original CAN frames.

use daqcore::{frame, superdbc, time};
use std::collections::BTreeMap;

#[derive(Clone, Copy)]
struct Character {
    timestamp: time::Time,
    byte: u8,
}

#[derive(Clone, Default)]
pub struct TaskNames {
    boards: BTreeMap<String, BTreeMap<u32, [Option<Character>; 16]>>,
}

impl TaskNames {
    pub fn clear(&mut self) {
        self.boards.clear();
    }

    pub fn observe(&mut self, frame: &frame::ParsedFrame) {
        let Some(message) = &frame.decoded else {
            return;
        };
        if !message.name().ends_with("_diag_name")
            || message.name() != format!("{}_diag_name", message.tx_node().to_lowercase())
        {
            return;
        }

        let Some((task_id, index, byte)) = decode_character(message) else {
            return;
        };
        let characters = self
            .boards
            .entry(message.tx_node().to_owned())
            .or_default()
            .entry(task_id)
            .or_insert([None; 16]);
        let character = Character {
            timestamp: frame.timestamp,
            byte,
        };
        // Late frames must not overwrite newer characters. A terminator also
        // invalidates the old suffix, retaining its timestamp against late data.
        let end = if byte == 0 {
            characters.len()
        } else {
            index + 1
        };
        for slot in &mut characters[index..end] {
            if slot.is_none_or(|previous| previous.timestamp <= character.timestamp) {
                *slot = Some(character);
            }
        }
    }

    pub fn name_at(&self, board: &str, task_id: u32, at: time::Time) -> Option<String> {
        let characters = self.boards.get(board)?.get(&task_id)?;
        let mut bytes = Vec::new();
        for character in characters {
            let character = (*character)?;
            // An expanded timeline may reach before the retained name baseline.
            // Show an unknown name rather than using information from the future.
            if character.timestamp > at {
                return None;
            }
            if character.byte == 0 {
                return Some(String::from_utf8_lossy(&bytes).into_owned());
            }
            bytes.push(character.byte);
        }
        None
    }
}

fn decode_character(message: &superdbc::decode::DecodedMessage) -> Option<(u32, usize, u8)> {
    let integer = |name: &str| match message.signal(name)?.raw() {
        superdbc::message::RawValue::Integer(value) => Some(value),
        _ => None,
    };
    let task_id = u32::try_from(integer("task_id")?).ok()?;
    let index = usize::try_from(integer("char_index")?).ok()?;
    let byte = u8::try_from(integer("character")?).ok()?;
    (index < 16).then_some((task_id, index, byte))
}
