use std::collections::BTreeMap;

use eframe::egui;

use crate::ui::diagnostics::TaskState::Running;

pub struct TaskDiagnostics {
    pub task_id: u32,
    pub name: String,
    pub state: TaskState,
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
        let mut boards = BTreeMap::new();
        boards.insert("main_module".to_string(), BoardDiagnostics{
            name: "main_module".to_string(),
            cpu_util: Some(67.420),
            capacity_exceeded: false,
            sample_count: 10,
            task_count: 1,
            tasks: vec![
                TaskDiagnostics {
                    task_id: 1,
                    name: "idle task".to_string(),
                    state: TaskState::Ready,
                    stack_hwm: 100,
                    cpu_util: Some(1.0),
                },
                TaskDiagnostics  {
                    task_id: 2,
                    name: "random stuff".to_string(),
                    state: TaskState::Running,
                    stack_hwm: 50,
                    cpu_util: Some(5.0),
                }
            ],
            timestamp_ms: 10000,
        },);
        Self {
        title: format!("Diagnostics #{}", instance_num),
        boards: boards,
        search: String::new(),
        selected_board: None,
        }
    }

    pub fn show(&mut self, ui: &mut egui::Ui) -> egui_tiles::UiResponse {
        ui.heading(&self.title);
        ui.separator();

        ui.horizontal(|ui| { 
            ui.label("Search:");
            ui.text_edit_singleline(&mut self.search);

            if ui.button("Clear").clicked() {
                self.search.clear();
            }
        });

        ui.add_space(8.0);

        egui::ScrollArea::vertical().show(ui, |ui| {
            let query = self.search.to_lowercase();
            for (board_name, board) in &self.boards {
            
            ui.label(egui::RichText::new(board_name).strong().size(16.0));

            let cpu_str = match board.cpu_util {
                Some(cpu) => format!("CPU: {:.1}%", cpu),
                None => "Invalid".to_string(),
                };
            

            ui.label(egui::RichText::new(cpu_str).strong());

            ui.label(egui::RichText::new(format!("({} ms)", board.timestamp_ms)).italics().weak());

            ui.add_space(4.0);

            egui::Frame::group(ui.style()).fill(ui.visuals().faint_bg_color)
            .corner_radius(egui::CornerRadius::same(8))
            .inner_margin(egui::Margin::symmetric(8, 6))
            .show(ui, |ui| {
                egui::Grid::new(format!("board_tasks_{}", board_name)).striped(true).spacing([24.0, 6.0])
                .show(ui, |ui| {
                    ui.strong("ID");
                    ui.strong("Task Name");
                    ui.strong("State");
                    ui.strong("Stack HWM");
                    ui.strong("CPU %");
                    ui.end_row();

                    for task in &board.tasks {
                        if !query.is_empty() && !board_name.to_lowercase().contains(&query) && !task.name.to_lowercase().contains(&query) {
                continue;
            }
                        ui.monospace(task.task_id.to_string());
                        ui.monospace(&task.name);
                        ui.label(task.state.as_str());
                        ui.monospace(format!("{} B", task.stack_hwm));

                        match task.cpu_util {
                            Some(cpu) => ui.monospace(format!("{:.1}%", cpu)),
                            None => ui.monospace("Invalid"),
                        };
                        ui.end_row();
                    }
                });
            });
            ui.add_space(12.0);
        }
        });
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