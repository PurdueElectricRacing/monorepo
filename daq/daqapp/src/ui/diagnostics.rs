use crate::telemetry;
use eframe::egui;
use std::collections::BTreeMap;

struct TaskDiagnostics {
    task_id: u32,
    name: String,
    state: String,
    stack_hwm: u16,
    cpu_util: Option<f32>,
}

#[derive(Default)]
struct BoardDiagnostics {
    cpu_util: Option<f32>,
    capacity_exceeded: bool,
    // Retained even when search hides every task, for search matching.
    tasks: Vec<TaskDiagnostics>,
    sample_count: Option<u32>,
    task_count: Option<u16>,
}

pub struct Diagnostics {
    pub title: String,
    pub search: String,
}

struct BoardSummary {
    sample_count: u32,
    task_count: u16,
    cpu_util: Option<f32>,
    capacity_exceeded: bool,
}

impl Diagnostics {
    pub fn new(instance_num: usize) -> Self {
        Self {
            title: format!("Diagnostics #{}", instance_num),
            search: String::new(),
        }
    }

    pub fn show(
        &mut self,
        ui: &mut egui::Ui,
        view: &telemetry::TelemetryView<'_>,
        task_names: &crate::task_names::TaskNames,
    ) -> egui_tiles::UiResponse {
        let boards = Self::snapshot(view, task_names);
        let cpu_history = Self::cpu_history(view);
        let start = view.timeline.start();
        let duration = view.timeline.end().secs(start).max(0.001);
        let playhead = view.timeline.setpoint().secs(start);
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

        let pixels = ui.available_width().max(1.0) as usize;
        egui_plot::Plot::new(format!("diagnostics_cpu_plot{}", self.title))
            .height(180.0)
            .y_axis_label("CPU Usage (%)")
            .include_y(0.0)
            .include_y(100.0)
            .allow_zoom([false, true])
            .allow_drag([false, true])
            .allow_scroll([false, true])
            .x_axis_label("Time")
            .x_axis_formatter(|mark, _| start.offset((mark.value * 1000.0).round() as i64).label())
            .legend(egui_plot::Legend::default())
            .show(ui, |plot_ui| {
                plot_ui.set_plot_bounds_x(0.0..=duration);
                for (board_name, history) in &cpu_history {
                    let points = telemetry::decimate(history, pixels);
                    plot_ui.line(egui_plot::Line::new(
                        board_name,
                        egui_plot::PlotPoints::from(points),
                    ));
                }
                plot_ui.vline(
                    egui_plot::VLine::new("Playhead", playhead).color(egui::Color32::YELLOW),
                );
            });

        ui.add_space(8.0);

        egui::ScrollArea::vertical().show(ui, |ui| {
            let query = self.search.to_lowercase();
            for (board_name, board) in &boards {
                let board_matches = board_name.to_lowercase().contains(&query);
                let matching_tasks: Vec<_> = board
                    .tasks
                    .iter()
                    .filter(|task| {
                        query.is_empty()
                            || board_matches
                            || task.name.to_lowercase().contains(&query)
                    })
                    .collect();
                if !query.is_empty() && !board_matches && matching_tasks.is_empty() {
                    continue;
                }
                ui.label(egui::RichText::new(board_name).strong().size(16.0));

                let cpu_str = match board.cpu_util {
                    Some(cpu) => format!("CPU: {:.1}%", cpu),
                    None => "Invalid".to_string(),
                };

                ui.label(egui::RichText::new(cpu_str).strong());
                if let Some(sample_count) = board.sample_count {
                    ui.label(format!("Sample #{}", sample_count));
                }
                if !board.capacity_exceeded
                    && let Some(task_count) = board.task_count
                {
                    ui.label(format!("Reported tasks: {}", task_count));
                }
                if board.capacity_exceeded {
                    ui.colored_label(
                        egui::Color32::YELLOW,
                        "Task snapshot unavailable: capacity exceeded",
                    );
                }
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

                                for task in &matching_tasks {
                                    ui.monospace(task.task_id.to_string());
                                    ui.monospace(&task.name);
                                    ui.label(&task.state);
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
        let state = msg
            .signal("state")?
            .enum_label()
            .map(str::to_owned)
            .unwrap_or_else(|| format!("Unknown ({state})"));

        Some(TaskDiagnostics {
            task_id,
            name: format!("Task {}", task_id),
            state,
            stack_hwm,
            cpu_util: Self::decode_cpu(cpu),
        })
    }

    fn decode_summary(msg: &daqcore::superdbc::decode::DecodedMessage) -> Option<BoardSummary> {
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

    fn snapshot(
        view: &telemetry::TelemetryView<'_>,
        task_names: &crate::task_names::TaskNames,
    ) -> BTreeMap<String, BoardDiagnostics> {
        let mut boards = BTreeMap::<String, BoardDiagnostics>::new();
        let names = task_names.names_at(view.setpoint_frames);

        // Task stats share a CAN identity: keep the latest frame per task ID,
        // not just the latest message for each board.
        for frame in view.setpoint_frames {
            let Some(message) = &frame.decoded else {
                continue;
            };
            let task = Self::decode_task(message);
            let summary = Self::decode_summary(message);
            if task.is_none() && summary.is_none() {
                continue;
            }
            let board = boards.entry(message.tx_node().to_owned()).or_default();

            if let Some(summary) = summary {
                board.sample_count = Some(summary.sample_count);
                board.task_count = Some(summary.task_count);
                board.cpu_util = summary.cpu_util;
                board.capacity_exceeded = summary.capacity_exceeded;
            }

            if let Some(task) = task {
                if let Some(existing) = board.tasks.iter_mut().find(|t| t.task_id == task.task_id) {
                    *existing = task;
                } else {
                    board.tasks.push(task);
                }
            }
        }

        for (board_name, board) in &mut boards {
            for task in &mut board.tasks {
                if let Some(name) =
                    names.name_at(board_name, task.task_id, view.timeline.setpoint())
                {
                    task.name = name;
                }
            }
        }
        boards
    }

    fn cpu_history(view: &telemetry::TelemetryView<'_>) -> BTreeMap<String, Vec<[f64; 2]>> {
        let mut history = BTreeMap::<String, Vec<[f64; 2]>>::new();
        for frame in view.all_frames {
            let Some(message) = &frame.decoded else {
                continue;
            };
            let Some(summary) = Self::decode_summary(message) else {
                continue;
            };
            if let Some(cpu) = summary.cpu_util {
                history
                    .entry(message.tx_node().to_owned())
                    .or_default()
                    .push([frame.timestamp.secs(view.timeline.start()), f64::from(cpu)]);
            }
        }
        history
    }
}
