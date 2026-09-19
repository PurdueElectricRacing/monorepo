pub struct Frozen<T> {
    pub rt_data: T,
    pub frozen_data: Option<T>,
}

impl<T: Clone> Frozen<T> {
    pub fn new(rt_data: T) -> Self {
        Self {
            rt_data,
            frozen_data: None,
        }
    }

    pub fn freeze(&mut self) {
        self.frozen_data = Some(self.rt_data.clone());
    }

    pub fn unfreeze(&mut self) {
        self.frozen_data = None;
    }

    pub fn get(&self) -> &T {
        if let Some(ref frozen) = self.frozen_data {
            frozen
        } else {
            &self.rt_data
        }
    }

    pub fn get_mut(&mut self) -> &mut T {
        &mut self.rt_data
    }

    pub fn apply_both<F>(&mut self, f: F)
    where
        F: Fn(&mut T),
    {
        f(&mut self.rt_data);
        if let Some(ref mut frozen) = self.frozen_data {
            f(frozen);
        }
    }
}
