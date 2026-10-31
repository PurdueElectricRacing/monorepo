pub fn clear_expectations(
    events: &mut HashMap<String, daqcore::can::driver::FilExpectationEvent>,
    order: &mut Vec<String>,
) {
    events.clear();
    order.clear();
}

pub fn update_expectations(
    events: &mut HashMap<String, daqcore::can::driver::FilExpectationEvent>,
    order: &mut Vec<String>,
    event: daqcore::can::driver::FilExpectationEvent,
) {
    if !events.contains_key(&event.check_id) {
        order.push(event.check_id.clone());
    }
    events.insert(event.check_id.clone(), event);
    while order.len() > FIL_EXPECTATION_HISTORY_LIMIT {
        let id = order.remove(0);
        events.remove(&id);
    }
}

use super::*;

impl FilControl {
    pub fn handle_can_message(&mut self, msg: &daqcore::can_thread::CanThreadEvent) {
        match msg {
            daqcore::can_thread::CanThreadEvent::FilGpio {
                board,
                port,
                pin,
                value,
                direction,
            } => {
                let Some(port) = FilGpioPort::parse(port) else {
                    return;
                };
                let state = self
                    .gpio_states
                    .entry((board.clone(), port, *pin))
                    .or_default();
                match direction {
                    daqcore::can_thread::FilGpioDirection::Input => state.input = *value,
                    daqcore::can_thread::FilGpioDirection::Output => state.output = *value,
                }
            }
            daqcore::can_thread::CanThreadEvent::FilExpectation(event) => update_expectations(
                &mut self.expectations,
                &mut self.expectation_order,
                event.clone(),
            ),
            daqcore::can_thread::CanThreadEvent::ConnectionSuccessful => {
                self.gpio_states.clear();
                clear_expectations(&mut self.expectations, &mut self.expectation_order);
            }
            daqcore::can_thread::CanThreadEvent::Disconnection
            | daqcore::can_thread::CanThreadEvent::ConnectionFailed(_) => self.gpio_states.clear(),
            _ => {}
        }
    }
}
