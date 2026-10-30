use eframe::egui::{self, accesskit::{Action::Expand, Role::ContentDeletion}};
use std::collections::{BTreeMap, VecDeque};

pub struct TaskDiagnostics {
    pub task_id: u32,
    pub name: String,
    pub state: TaskState,
    pub stack_hwm: u16,
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
    pub cpu_history: VecDeque<(f64, f64)>,
}

pub struct Diagnostics {
    pub title: String,

    pub boards: BTreeMap<String, BoardDiagnostics>,
    pub search: String,
    pub selected_board: Option<String>,
}

pub struct BoardSummary {
    pub sample_count: u32,
    pub task_count: u16,
    pub cpu_util: Option<f32>,
    pub capacity_exceeded: bool,
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

    pub fn show(
        &mut self,
        ui: &mut egui::Ui,
        view: &crate::telemetry::TelemetryView<'_>,
    ) -> egui_tiles::UiResponse {
        self.update_from_view(view);
        ui.heading(&self.title);
        ui.label(format!(
            "CAN frames up to timeline cursor: {}",
            view.setpoint_frames.len()
        ));

        ui.separator();

        ui.horizontal(|ui| {
            ui.label("Search:");
            ui.text_edit_singleline(&mut self.search);

            if ui.button("Clear").clicked() {
                self.search.clear();
            }
        });

        ui.add_space(8.0);

        egui_plot::Plot::new(format!("diagnostics_cpu_plot{}", self.title))
            .height(180.0)
            .y_axis_label("CPU Usage (%)")
            .include_y(0.0)
            .include_y(100.0)
            .legend(egui_plot::Legend::default())
            .show(ui, |plot_ui| {
                for (board_name, board) in &self.boards {
                    let points: egui_plot::PlotPoints = board
                        .cpu_history
                        .iter()
                        .map(|(time, cpu)| [*time, *cpu])
                        .collect();

                    plot_ui.line(egui_plot::Line::new(board_name, points));
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
                ui.add_space(4.0);

                egui::Frame::group(ui.style())
                    .fill(ui.visuals().faint_bg_color)
                    .corner_radius(egui::CornerRadius::same(8))
                    .inner_margin(egui::Margin::symmetric(8, 6))
                    .show(ui, |ui| {
                        egui::Grid::new(format!("board_tasks_{}", board_name))
                            .striped(true)
                            .spacing([24.0, 6.0])
                            .show(ui, |ui| {
                                ui.strong("ID");
                                ui.strong("Task Name");
                                ui.strong("State");
                                ui.strong("Stack HWM");
                                ui.strong("CPU %");
                                ui.end_row();

                                for task in &board.tasks {
                                    if !query.is_empty()
                                        && !board_name.to_lowercase().contains(&query)
                                        && !task.name.to_lowercase().contains(&query)
                                    {
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
    fn decode_cpu(raw: u8) -> Option<f32> {
        match raw {
            0..=100 => Some(raw as f32),
            _ => None,
        }
    }

    fn integer_signal(msg: &daqcore::superdbc::decode::DecodedMessage, name: &str) -> Option<i128> {
        match msg.signal(name)?.raw() {
            daqcore::superdbc::message::RawValue::Integer(value) => Some(value),
            _ => None,
        }
    }

    fn decode_task(msg: &daqcore::superdbc::decode::DecodedMessage) -> Option<TaskDiagnostics> {
        let expected_name = format!("{}_diag_task", msg.tx_node().to_lowercase());

        if msg.name() != expected_name {
            return None;
        }
        let task_id = u32::try_from(Self::integer_signal(msg, "task_id")?).ok()?;
        let stack_hwm = u16::try_from(Self::integer_signal(msg, "stack_hwm")?).ok()?;
        let cpu = u8::try_from(Self::integer_signal(msg, "cpu_percent")?).ok()?;
        let state = u8::try_from(Self::integer_signal(msg, "state")?).ok()?;

        Some(TaskDiagnostics {
            task_id,
            name: format!("Task {}", task_id),
            state: TaskState::from_raw(state),
            stack_hwm,
            cpu_util: Self::decode_cpu(cpu),
        })
    }

    fn decode_summary(
        msg: &daqcore::superdbc::decode::DecodedMessage,
    ) -> Option<BoardSummary> {
        let expected_name = format!("{}_diag_summary", msg.tx_node().to_lowercase());

        if msg.name() != expected_name {
            return None;
        }

       let sample_count = Self::integer_signal(msg, "sample_count")?;
       let task_count = Self::integer_signal(msg, "task_count")?;
       let cpu = Self::integer_signal(msg, "cpu_percent")?;
       let capacity = Self::integer_signal(msg, "capacity_exceeded")?;

       let sample_count = u32::try_from(sample_count).ok()?;
       let task_count = u16::try_from(task_count).ok()?;
       let cpu = u8::try_from(cpu).ok()?;

       let capacity_exceeded = match capacity {
           0 => false,
           1 => true,
           _ => return None,
       };

       Some(BoardSummary {
           sample_count,
           task_count,
           cpu_util: Self::decode_cpu(cpu),
           capacity_exceeded,
       })
    }
    
    fn update_from_view(&mut self, view: &crate::telemetry::TelemetryView<'_>) {
        self.boards.clear();

        for frame in view.setpoint_frames {
            let msg = match &frame.decoded {
                Some(msg) => msg,
                None => continue,
            };

            let task = match Self::decode_task(msg) {
                Some(task) => task,
                None => continue,
            };

            let board_name = msg.tx_node().to_string();

            if !self.boards.contains_key(&board_name) {
                self.boards.insert(board_name.clone(), BoardDiagnostics {
                    name: board_name.clone(),
                    cpu_util: None,
                    capacity_exceeded: false,
                    tasks: Vec::new(),
                    sample_count: 0,
                    task_count: 0,
                    timestamp_ms: 0,
                    cpu_history: VecDeque::new(), 
                },
            );
                
            }
            let board = self.boards.get_mut(&board_name).unwrap();

            let mut existing_index = None;

            for i in 0 .. board.tasks.len() {
                if board.tasks[i].task_id == task.task_id {
                    existing_index = Some(i);
                    break;
                }
            }

            match existing_index {
                Some(i) => board.tasks[i] = task,
                None => board.tasks.push(task),
            }

            board.task_count = board.tasks.len() as u32;
        }
    }

}

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum TaskState {
    Running,
    Ready,
    Blocked,
    Suspended,
    Deleted,
    Invalid,
}

impl TaskState {
    pub fn from_raw(raw: u8) -> Self {
        match raw {
            0 => Self::Running,
            1 => Self::Ready,
            2 => Self::Blocked,
            3 => Self::Suspended,
            4 => Self::Deleted,
            _ => Self::Invalid,
        }
    }

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
