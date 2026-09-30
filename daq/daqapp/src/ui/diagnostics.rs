use eframe::egui;

pub struct TaskDiagnostics {
    pub task_id: u32,
    pub name: String,
    pub stack_hwm: u32,
    pub cpu_util: Option<f32>,
}

pub struct Diagnostics {
    pub title: String,
    pub tasks: Vec<TaskDiagnostics>,
}

