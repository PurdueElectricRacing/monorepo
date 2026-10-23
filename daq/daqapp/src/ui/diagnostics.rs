use std::collections::BTreeMap;

use eframe::egui;

pub struct TaskDiagnostics {
    pub task_id: u32,
    pub name: String,
    pub stack_hwm: u32,
    pub cpu_util: Option<f32>,
}
pub struct BoardDiagnostics {
    pub name: String,
    pub cpu_util: Option<f32>,
    pub capacity_exceeded: bool,  
    pub tasks: Vec<TaskDiagnostics>,
    pub sample_count: u32,
    pub task_count: u32,
    pub timestamp_ms: u32,
}

pub struct Diagnostics {
    pub title: String,

    pub boards: BTreeMap<String, BoardDiagnostics>,
    pub search: String,
    pub selected_board: Option<String>,
}

impl Diagnostics {
    pub fn new(instance_num: usize) -> Self {
        Self {
        title: format!("Diagnostics #{}", instance_num),
        boards: BTreeMap::new(),
        search: String::new(),
        selected_board: None,
        }
    }

    pub fn show(&mut self, ui: &mut egui::Ui) -> egui_tiles::UiResponse {
        ui.heading(&self.title);
        egui_tiles::UiResponse::None
    }

    pub fn handle_can_message(&mut self, _msg: &crate::messages::MsgFromCan) {
    
    }
}


#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum TaskState {
    Running,
    Ready,
    Blocked,
    Suspended,
    Deleted,
    Invalid
} 

impl TaskState {
    pub fn as_str(&self) -> &'static str {
        match self {
            TaskState::Running => "Running",
            TaskState::Ready => "Ready",
            TaskState::Blocked => "Blocked",
            TaskState::Suspended => "Suspended",
            TaskState::Deleted => "Deleted",
            TaskState::Invalid => "Invalid",
        }
    }
}