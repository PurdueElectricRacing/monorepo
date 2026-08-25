use crate::widget_constructor;

pub struct WidgetIds {
    counters: std::collections::HashMap<widget_constructor::WidgetKind, usize>,
}

impl WidgetIds {
    pub fn new() -> Self {
        Self {
            counters: std::collections::HashMap::new(),
        }
    }

    pub fn next(&mut self, kind: widget_constructor::WidgetKind) -> usize {
        let counter = self.counters.entry(kind).or_insert(1);
        let id = *counter;
        *counter += 1;
        id
    }
}
