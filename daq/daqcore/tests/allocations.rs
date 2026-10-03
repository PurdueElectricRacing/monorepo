use std::{
    alloc::{GlobalAlloc, Layout, System},
    cell::Cell,
};
thread_local! { static TRACK: Cell<bool> = const { Cell::new(false) }; static COUNT: Cell<usize> = const { Cell::new(0) }; }
struct Allocator;
fn count() {
    let _ = TRACK.try_with(|t| {
        if t.get() {
            let _ = COUNT.try_with(|c| c.set(c.get() + 1));
        }
    });
}
unsafe impl GlobalAlloc for Allocator {
    unsafe fn alloc(&self, l: Layout) -> *mut u8 {
        count();
        unsafe { System.alloc(l) }
    }
    unsafe fn alloc_zeroed(&self, l: Layout) -> *mut u8 {
        count();
        unsafe { System.alloc_zeroed(l) }
    }
    unsafe fn realloc(&self, p: *mut u8, l: Layout, n: usize) -> *mut u8 {
        count();
        unsafe { System.realloc(p, l, n) }
    }
    unsafe fn dealloc(&self, p: *mut u8, l: Layout) {
        unsafe { System.dealloc(p, l) }
    }
}
#[global_allocator]
static A: Allocator = Allocator;
#[test]
fn decode_encode_and_resolve_allocate_nothing() {
    use daqcore::superdbc::*;
    let db = SuperDbc::from_str(include_str!("fixtures/superdbc.json")).unwrap();
    let b = db.bus("VCAN").unwrap();
    let m = b.messages.iter().find(|m| m.signals.len() == 16).unwrap();
    let decoder = Decoder::new(&db, b.bus_id).unwrap();
    let values = [0.; 16];
    COUNT.with(|c| c.set(0));
    TRACK.with(|t| t.set(true));
    for _ in 0..100 {
        let f = std::hint::black_box(decoder.decode(m.id, &[0; 8]).unwrap().unwrap());
        let view = f.view(&db).unwrap();
        for (_, s) in &view.signals {
            std::hint::black_box(s.value);
        }
        std::hint::black_box(m.encode(&values, EncodePolicy::Clamp).unwrap());
    }
    TRACK.with(|t| t.set(false));
    assert_eq!(COUNT.with(|c| c.get()), 0);
    assert!(std::mem::size_of::<DecodedFrame>() <= 1100);
}
