use daqcore::{
    can::{BusId, CanFrame, MessageId},
    superdbc::*,
};
use serde_json::{Value, json};
const FLEET: &str = include_str!("fixtures/superdbc.json");
fn small(signal: Value, len: u8) -> Value {
    json!({"content_hash":"a".repeat(64),"versions":{"schema_version":1,"hash":"test"},"buses":{"X":{"bus_id":3,"baud_rate":500000,"nodes":[{"name":"N","is_external":false}],"messages":[{"id":1,"is_extended_id":false,"message_name":"M","transmitter":"N","receivers":[],"length_bytes":len,"nominal_period_ms":null,"priority":1,"description":"","signals":[signal]}]}}})
}
fn signal(ty: &str, start: u8, bits: u8, order: &str) -> Value {
    json!({"signal_name":"S","description":"","data_type":ty,"raw_type":ty,"start_bit":start,"bit_length":bits,"byte_order":order,"scale":1.0,"offset":0.0,"limits":null,"unit":"","choices":null})
}
fn load(v: Value) -> SuperDbc {
    SuperDbc::from_str(&v.to_string()).unwrap()
}
#[test]
fn fleet_matches_old_decoder_and_raw_round_trips() {
    let db = SuperDbc::from_str(FLEET).unwrap();
    assert_eq!(
        db.buses().iter().map(|b| b.messages.len()).sum::<usize>(),
        145
    );
    for bus in db.buses() {
        let path = std::path::Path::new(env!("CARGO_MANIFEST_DIR"))
            .join("tests/fixtures")
            .join(format!("{}.dbc", bus.name));
        let old = can_decode::Parser::from_dbc_file(&path).unwrap();
        let decoder = Decoder::new(&db, bus.bus_id).unwrap();
        let mut rng = 0x123456789abcdefu64;
        for msg in &bus.messages {
            for trial in 0..16 {
                rng ^= rng << 13;
                rng ^= rng >> 7;
                rng ^= rng << 17;
                let data = if trial == 0 {
                    [0; 8]
                } else if trial == 1 {
                    [255; 8]
                } else {
                    rng.to_le_bytes()
                };
                let frame = decoder
                    .decode(msg.id, &data[..msg.length_bytes as usize])
                    .unwrap()
                    .unwrap();
                let view = frame.view(&db).unwrap();
                let oracle = old.decode_msg(msg.id.to_wire_u32(), frame.data()).unwrap();
                assert_eq!(view.name, oracle.name);
                let mut raw = Vec::new();
                for (si, s) in msg.signals.iter().enumerate() {
                    let value = view.signals.at(si).unwrap().value;
                    let expected = &oracle.signals[&s.name].value;
                    assert_eq!(value.raw, expected.raw, "{}:{}", msg.name, s.name);
                    if expected.physical.is_nan() {
                        assert!(value.physical.is_nan());
                    } else {
                        assert_eq!(value.physical, expected.physical, "{}:{}", msg.name, s.name);
                    }
                    // The v1 generator explicitly supplies bool OFF/ON, unlike its DBC.
                    if let Some(label) = &expected.enum_label {
                        assert_eq!(value.enum_label, Some(label.as_str()));
                    }
                    raw.push(s.raw_value(value.raw_bits));
                }
                let encoded = msg.encode_raw(&raw).unwrap();
                let back = decoder.decode_frame(&encoded).unwrap().unwrap();
                for si in 0..msg.signals.len() {
                    assert_eq!(
                        frame.signal(si).unwrap().raw_bits,
                        back.signal(si).unwrap().raw_bits
                    );
                }
            }
        }
    }
}
#[test]
fn independent_motorola_vectors_and_short_payloads() {
    let db = load(small(signal("unsigned", 7, 16, "big_endian"), 2));
    let bus = db.buses()[0].bus_id;
    let decoder = Decoder::new(&db, bus).unwrap();
    let m = &db.buses()[0].messages[0];
    let f = decoder.decode(m.id, &[0x12, 0x34]).unwrap().unwrap();
    assert_eq!(f.signal(0).unwrap().raw_bits, 0x1234);
    assert_eq!(
        m.encode_raw(&[RawValue::U64(0x1234)]).unwrap().data(),
        &[0x12, 0x34]
    );
    assert!(
        decoder
            .decode(m.id, &[0x12])
            .unwrap()
            .unwrap()
            .signal(0)
            .is_none()
    );
    let last = load(small(signal("unsigned", 55, 16, "big_endian"), 8));
    assert!(
        last.buses()[0].messages[0]
            .encode_raw(&[RawValue::U64(0xffff)])
            .is_ok()
    );
    assert!(
        SuperDbc::from_str(&small(signal("unsigned", 0, 2, "big_endian"), 1).to_string()).is_err()
    );
}
#[test]
fn exact_full_width_signed_unsigned_and_choices() {
    for (ty, value) in [
        ("unsigned", RawValue::U64(u64::MAX)),
        ("signed", RawValue::I64(i64::MIN)),
    ] {
        let mut s = signal(ty, 0, 64, "little_endian");
        let key = if ty == "unsigned" {
            u64::MAX.to_string()
        } else {
            i64::MIN.to_string()
        };
        s["choices"] = json!({key:"edge"});
        let db = load(small(s, 8));
        let b = &db.buses()[0];
        let m = &b.messages[0];
        let encoded = m.encode_raw(&[value]).unwrap();
        let f = Decoder::new(&db, b.bus_id)
            .unwrap()
            .decode_frame(&encoded)
            .unwrap()
            .unwrap();
        assert_eq!(m.signals[0].raw_value(f.signal(0).unwrap().raw_bits), value);
        assert_eq!(
            f.view(&db).unwrap().signals.at(0).unwrap().value.enum_label,
            Some("edge")
        );
    }
}
#[test]
fn capacity_is_64_and_partial_slots_stay_positional() {
    let mut v = small(signal("unsigned", 0, 1, "little_endian"), 8);
    v["buses"]["X"]["messages"][0]["signals"] = Value::Array(
        (0..64)
            .map(|i| {
                let mut s = signal("unsigned", i, 1, "little_endian");
                s["signal_name"] = json!(format!("S{i}"));
                s
            })
            .collect(),
    );
    let db = load(v);
    let b = &db.buses()[0];
    let f = Decoder::new(&db, b.bus_id)
        .unwrap()
        .decode(b.messages[0].id, &[255])
        .unwrap()
        .unwrap();
    assert_eq!(f.n_signals, 64);
    assert!(f.signal(7).is_some());
    assert!(f.signal(8).is_none());
    assert!(f.signal(63).is_none());
    assert!(f.signal(64).is_none());
}
#[test]
fn checked_identity_preserves_bus_extendedness_and_dlc() {
    let id = MessageId::from_parts(true, 1).unwrap();
    let f = CanFrame::new(id, &[7])
        .unwrap()
        .with_bus(BusId::new(1).unwrap());
    assert_eq!(f.len(), 1);
    assert_eq!(f.log_identity(BusId::new(0).unwrap()).unwrap(), 0xc0000001);
    let g = CanFrame::from_log_identity(0xc0000001, &[0; 8]).unwrap();
    assert!(g.id.is_extended());
    assert_eq!(g.id.raw(), 1);
    assert_eq!(g.bus, f.bus);
    assert_ne!(id, MessageId::from_parts(false, 1).unwrap());
    assert!(MessageId::from_parts(false, 0x800).is_err());
    assert!(MessageId::from_wire_u32(0x40000001).is_err());
    assert!(CanFrame::from_log_identity(0x20000001, &[0; 8]).is_err());
    assert!(CanFrame::new(id, &[0; 9]).is_err());
    assert!(
        f.with_bus(BusId::new(2).unwrap())
            .log_identity(BusId::new(0).unwrap())
            .is_err()
    );
}
#[test]
fn generations_reject_stale_frames_but_clones_share_identity() {
    let db = load(small(signal("unsigned", 0, 8, "little_endian"), 1));
    let b = &db.buses()[0];
    let f = Decoder::new(&db, b.bus_id)
        .unwrap()
        .decode(b.messages[0].id, &[42])
        .unwrap()
        .unwrap();
    assert!(f.view(&db.clone()).is_some());
    let other = load(small(signal("unsigned", 0, 8, "little_endian"), 1));
    assert!(f.view(&other).is_none());
    assert!(Decoder::new(&db, BusId::new(0).unwrap()).is_none());
}
#[test]
fn encode_policies_scaling_float_and_errors() {
    let mut s = signal("signed", 0, 8, "little_endian");
    s["scale"] = json!(-0.5);
    s["offset"] = json!(2.0);
    let db = load(small(s, 1));
    let b = &db.buses()[0];
    let m = &b.messages[0];
    assert_eq!(m.signals[0].physical_range(), (-61.5, 66.0));
    let f = m.encode(&[3.5], EncodePolicy::Reject).unwrap();
    assert_eq!(f.data(), &[253]);
    assert!(m.encode(&[1000.], EncodePolicy::Reject).is_err());
    assert_eq!(
        m.encode(&[1000.], EncodePolicy::Clamp).unwrap().data(),
        &[128]
    );
    assert!(m.encode(&[f64::NAN], EncodePolicy::Clamp).is_err());
    assert!(m.encode(&[], EncodePolicy::Reject).is_err());
    assert!(m.encode_raw(&[RawValue::U64(1)]).is_err());
    let db = load(small(signal("float32", 0, 32, "little_endian"), 4));
    let b = &db.buses()[0];
    let f = b.messages[0].encode(&[1.5], EncodePolicy::Reject).unwrap();
    assert_eq!(f.data(), &1.5f32.to_le_bytes());
    let remote = CanFrame::remote(b.messages[0].id, 4).unwrap();
    assert!(
        Decoder::new(&db, b.bus_id)
            .unwrap()
            .decode_frame(&remote)
            .unwrap()
            .is_none()
    );
}
#[test]
fn strict_schema_and_semantic_rejections() {
    let original = small(signal("unsigned", 0, 8, "little_endian"), 1);
    for case in 0..12 {
        let mut v = original.clone();
        match case {
            0 => {
                v["versions"]["schema_version"] = json!(2);
            }
            1 => {
                v["unexpected"] = json!(true);
            }
            2 => {
                v["buses"]["X"]["messages"][0]["signals"][0]
                    .as_object_mut()
                    .unwrap()
                    .remove("choices");
            }
            3 => {
                v["buses"]["X"]["messages"][0]["signals"][0]["scale"] = json!(0);
            }
            4 => {
                v["buses"]["X"]["messages"][0]["signals"][0]["limits"] = json!({"min":2,"max":1});
            }
            5 => {
                v["buses"]["X"]["messages"][0]["signals"][0]["choices"] = json!({"01":"bad"});
            }
            6 => {
                v["buses"]["X"]["messages"][0]["signals"][0]["choices"] = json!({"256":"bad"});
            }
            7 => {
                v["buses"]["X"]["messages"][0]["id"] = json!(4095);
            }
            8 => {
                let s = v["buses"]["X"]["messages"][0]["signals"][0].clone();
                let mut s2 = s.clone();
                s2["signal_name"] = json!("overlap");
                v["buses"]["X"]["messages"][0]["signals"] = json!([s, s2]);
            }
            9 => {
                v["buses"]["X"]["messages"][0]["transmitter"] = json!("missing");
            }
            10 => {
                v["buses"]["X"]["messages"][0]["signals"][0]["raw_type"] = json!("float32");
            }
            _ => {
                let b = v["buses"]["X"].clone();
                v["buses"]["Y"] = b;
            }
        }
        assert!(SuperDbc::from_str(&v.to_string()).is_err(), "case {case}");
    }
}

#[test]
fn duplicate_json_keys_and_mutated_encoding_layouts_are_rejected() {
    let v = small(signal("unsigned", 0, 8, "little_endian"), 1);
    let json = v.to_string();
    let choices = json.replace("\"choices\":null", "\"choices\":{\"0\":\"a\",\"0\":\"b\"}");
    assert!(SuperDbc::from_str(&choices).is_err());
    let buses = json.replace("\"X\":", &format!("\"X\":{},\"X\":", v["buses"]["X"]));
    assert!(SuperDbc::from_str(&buses).is_err());
    let db = load(v);
    let original = &db.buses()[0].messages[0];
    let mut m = original.clone();
    m.length_bytes = 9;
    assert_eq!(
        m.encode(&[1.0], EncodePolicy::Reject),
        Err(EncodeError::InvalidDefinition)
    );
    m = original.clone();
    m.signals[0].bit_length = 0;
    assert_eq!(
        m.encode_raw(&[RawValue::U64(1)]),
        Err(EncodeError::InvalidDefinition)
    );
    m = original.clone();
    m.signals[0].byte_order = ByteOrder::BigEndian;
    assert_eq!(
        m.encode(&[1.0], EncodePolicy::Reject),
        Err(EncodeError::InvalidDefinition)
    );
}
