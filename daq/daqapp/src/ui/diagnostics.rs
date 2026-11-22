use crate::telemetry;
use eframe::egui;
use std::collections::{BTreeMap, BTreeSet};

// Matches configMAX_TASK_NAME_LEN in firmware/common/rtos/FreeRTOSConfig.h.
const MAX_TASK_NAME_LEN: usize = 16;

struct TaskDiagnostics {
    task_id: u32,
    name: String,
    state: String,
    stack_hwm: u16,
    cpu_util: Option<u8>,
}

#[derive(Default)]
struct BoardDiagnostics {
    cpu_util: Option<u8>,
    capacity_exceeded: bool,
    // Retained even when search hides every task, for search matching.
    tasks: BTreeMap<u32, TaskDiagnostics>,
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
    cpu_util: Option<u8>,
    capacity_exceeded: bool,
}

impl Diagnostics {
    pub fn new(instance_num: usize) -> Self {
        Self {
            title: format!("Diagnostics #{instance_num}"),
            search: String::new(),
        }
    }

    pub fn show(
        &mut self,
        ui: &mut egui::Ui,
        view: &telemetry::TelemetryView<'_>,
    ) -> egui_tiles::UiResponse {
        let boards = Self::snapshot(view);
        let cpu_history = Self::cpu_history(view);

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
        self.show_cpu_plot(ui, view, &cpu_history);
        ui.add_space(8.0);

        let query = self.search.to_lowercase();
        egui::ScrollArea::vertical().show(ui, |ui| {
            for (board_name, board) in &boards {
                self.show_board(ui, board_name, board, &query);
            }
        });
        egui_tiles::UiResponse::None
    }

    fn show_cpu_plot(
        &self,
        ui: &mut egui::Ui,
        view: &telemetry::TelemetryView<'_>,
        cpu_history: &BTreeMap<String, Vec<[f64; 2]>>,
    ) {
        let start = view.timeline.start();
        let duration = view.timeline.end().secs(start).max(0.001);
        let playhead = view.timeline.setpoint().secs(start);
        // Available width is clamped to >= 1.0 before casting to pixel count.
        let pixels = ui.available_width().max(1.0) as usize;

        egui_plot::Plot::new((&self.title, "diagnostics_cpu_plot"))
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
                for (board_name, history) in cpu_history {
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
    }

    fn show_board(
        &self,
        ui: &mut egui::Ui,
        board_name: &str,
        board: &BoardDiagnostics,
        query: &str,
    ) {
        let board_matches = board_name.to_lowercase().contains(query);
        let matching_tasks: Vec<_> = board
            .tasks
            .values()
            .filter(|task| {
                query.is_empty() || board_matches || task.name.to_lowercase().contains(query)
            })
            .collect();
        if !query.is_empty() && !board_matches && matching_tasks.is_empty() {
            return;
        }

        ui.label(egui::RichText::new(board_name).strong().size(16.0));

        let cpu_str = match board.cpu_util {
            Some(cpu) => format!("CPU: {cpu}%"),
            None => "CPU: Invalid".to_string(),
        };
        ui.label(egui::RichText::new(cpu_str).strong());

        if let Some(sample_count) = board.sample_count {
            ui.label(format!("Sample #{sample_count}"));
        }
        if board.capacity_exceeded {
            ui.colored_label(
                egui::Color32::YELLOW,
                "Task snapshot unavailable: capacity exceeded",
            );
            ui.add_space(12.0);
            return;
        }
        if let Some(task_count) = board.task_count {
            ui.label(format!("Reported tasks: {task_count}"));
        }

        ui.add_space(4.0);
        self.show_task_table(ui, board_name, &matching_tasks);
        ui.add_space(12.0);
    }

    fn show_task_table(&self, ui: &mut egui::Ui, board_name: &str, tasks: &[&TaskDiagnostics]) {
        egui::Frame::group(ui.style())
            .fill(ui.visuals().faint_bg_color)
            .corner_radius(egui::CornerRadius::same(8))
            .inner_margin(egui::Margin::symmetric(8, 6))
            .show(ui, |ui| {
                egui::Grid::new((&self.title, "board_tasks", board_name))
                    .striped(true)
                    .spacing([24.0, 6.0])
                    .show(ui, |ui| {
                        ui.strong("ID");
                        ui.strong("Task Name");
                        ui.strong("State");
                        ui.strong("Stack HWM");
                        ui.strong("CPU %");
                        ui.end_row();

                        for task in tasks {
                            ui.monospace(task.task_id.to_string());
                            ui.monospace(&task.name);
                            ui.label(&task.state);
                            ui.monospace(format!("{} B", task.stack_hwm));
                            match task.cpu_util {
                                Some(cpu) => ui.monospace(format!("{cpu}%")),
                                None => ui.monospace("Invalid"),
                            };
                            ui.end_row();
                        }
                    });
            });
    }

    fn decode_cpu(raw: u8) -> Option<u8> {
        (raw <= 100).then_some(raw)
    }

    fn integer_signal(msg: &daqcore::superdbc::decode::DecodedMessage, name: &str) -> Option<i128> {
        match msg.signal(name)?.raw() {
            daqcore::superdbc::message::RawValue::Integer(value) => Some(value),
            _ => None,
        }
    }

    fn matches_diag_message(msg: &daqcore::superdbc::decode::DecodedMessage, suffix: &str) -> bool {
        msg.name()
            .strip_suffix(suffix)
            .is_some_and(|prefix| prefix.eq_ignore_ascii_case(msg.tx_node()))
    }

    fn decode_task(msg: &daqcore::superdbc::decode::DecodedMessage) -> Option<TaskDiagnostics> {
        if !Self::matches_diag_message(msg, "_diag_task") {
            return None;
        }
        let task_id = u32::try_from(Self::integer_signal(msg, "task_id")?).ok()?;
        let stack_hwm = u16::try_from(Self::integer_signal(msg, "stack_hwm")?).ok()?;
        let cpu = u8::try_from(Self::integer_signal(msg, "cpu_percent")?).ok()?;
        let state_signal = msg.signal("state")?;
        let state_raw = match state_signal.raw() {
            daqcore::superdbc::message::RawValue::Integer(value) => u8::try_from(value).ok()?,
            _ => return None,
        };
        let state = state_signal
            .enum_label()
            .map(str::to_owned)
            .unwrap_or_else(|| format!("Unknown ({state_raw})"));

        Some(TaskDiagnostics {
            task_id,
            name: format!("Task {task_id}"),
            state,
            stack_hwm,
            cpu_util: Self::decode_cpu(cpu),
        })
    }

    fn decode_summary(msg: &daqcore::superdbc::decode::DecodedMessage) -> Option<BoardSummary> {
        if !Self::matches_diag_message(msg, "_diag_summary") {
            return None;
        }

        let sample_count = u32::try_from(Self::integer_signal(msg, "sample_count")?).ok()?;
        let task_count = u16::try_from(Self::integer_signal(msg, "task_count")?).ok()?;
        let cpu = u8::try_from(Self::integer_signal(msg, "cpu_percent")?).ok()?;
        let capacity_exceeded = Self::integer_signal(msg, "capacity_exceeded")? != 0;

        Some(BoardSummary {
            sample_count,
            task_count,
            cpu_util: Self::decode_cpu(cpu),
            capacity_exceeded,
        })
    }

    fn decode_name_char(
        msg: &daqcore::superdbc::decode::DecodedMessage,
    ) -> Option<(u32, usize, u8)> {
        if !Self::matches_diag_message(msg, "_diag_name") {
            return None;
        }
        let task_id = u32::try_from(Self::integer_signal(msg, "task_id")?).ok()?;
        let index = usize::try_from(Self::integer_signal(msg, "char_index")?).ok()?;
        let byte = u8::try_from(Self::integer_signal(msg, "character")?).ok()?;
        (index < MAX_TASK_NAME_LEN).then_some((task_id, index, byte))
    }

    fn assemble_name(chars: &[Option<u8>; MAX_TASK_NAME_LEN]) -> Option<String> {
        let mut bytes = Vec::new();
        for slot in chars {
            let byte = (*slot)?;
            if byte == 0 {
                return Some(String::from_utf8_lossy(&bytes).into_owned());
            }
            bytes.push(byte);
        }
        None
    }

    fn snapshot(view: &telemetry::TelemetryView<'_>) -> BTreeMap<String, BoardDiagnostics> {
        let mut boards = BTreeMap::<String, BoardDiagnostics>::new();
        let mut seen_in_cycle = BTreeMap::<String, BTreeSet<u32>>::new();
        let mut name_chars =
            BTreeMap::<String, BTreeMap<u32, [Option<u8>; MAX_TASK_NAME_LEN]>>::new();

        // Task stats and name characters share CAN identities: keep the latest
        // state per task ID across the rolling window up to the setpoint.
        for frame in view.setpoint_frames {
            let Some(message) = &frame.decoded else {
                continue;
            };
            let board_name = message.tx_node().to_owned();

            if let Some((task_id, index, byte)) = Self::decode_name_char(message) {
                let chars = name_chars
                    .entry(board_name)
                    .or_default()
                    .entry(task_id)
                    .or_insert([None; MAX_TASK_NAME_LEN]);
                if let Some(slot) = chars.get_mut(index) {
                    *slot = Some(byte);
                }
            } else if let Some(task) = Self::decode_task(message) {
                seen_in_cycle
                    .entry(board_name.clone())
                    .or_default()
                    .insert(task.task_id);
                boards
                    .entry(board_name)
                    .or_default()
                    .tasks
                    .insert(task.task_id, task);
            } else if let Some(summary) = Self::decode_summary(message) {
                let board = boards.entry(board_name.clone()).or_default();
                board.sample_count = Some(summary.sample_count);
                board.task_count = Some(summary.task_count);
                board.cpu_util = summary.cpu_util;
                board.capacity_exceeded = summary.capacity_exceeded;

                let seen = seen_in_cycle.remove(&board_name).unwrap_or_default();
                if summary.capacity_exceeded {
                    board.tasks.clear();
                } else {
                    board.tasks.retain(|id, _| seen.contains(id));
                }
            }
        }

        for (board_name, board) in &mut boards {
            let Some(board_names) = name_chars.get(board_name) else {
                continue;
            };
            for task in board.tasks.values_mut() {
                if let Some(chars) = board_names.get(&task.task_id)
                    && let Some(name) = Self::assemble_name(chars)
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
