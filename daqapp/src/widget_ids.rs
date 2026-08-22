use crate::widget_constructor;

pub struct WidgetIds {
    counters: std::collections::HashMap<
        std::mem::Discriminant<widget_constructor::WidgetConstructor>,
        usize,
    >,
}

impl WidgetIds {
    pub fn new() -> Self {
        Self {
            counters: std::collections::HashMap::new(),
        }
    }

    pub fn next(&mut self, kind: widget_constructor::WidgetConstructor) -> usize {
        let disc = if matches!(kind, widget_constructor::WidgetConstructor::ScopeEmpty) {
            std::mem::discriminant(&widget_constructor::WidgetConstructor::Scope {
                msg_id: 0,
                msg_name: String::new(),
                signal_name: String::new(),
            })
        } else {
            std::mem::discriminant(&kind)
        };

        let counter = self.counters.entry(disc).or_insert(1);
        let id = *counter;
        *counter += 1;
        id
    }
}
