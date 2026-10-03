use super::CanThreadEvent;
/// The worker's only event sender. A missing receiver is a normal shutdown condition.
pub(super) struct Events(pub std::sync::mpsc::Sender<CanThreadEvent>);
impl Events {
    pub fn emit(&self, event: CanThreadEvent) -> bool {
        self.0.send(event).is_ok()
    }
}
