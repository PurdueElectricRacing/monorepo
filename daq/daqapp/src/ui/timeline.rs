use crate::app::DAQApp;
use daqcore::{Time, timeline::Track};
use eframe::egui;
pub fn show(app: &mut DAQApp, ctx: &egui::Context) {
    egui::TopBottomPanel::top("shared_timeline").show(ctx, |ui| {
        let before = (
            app.session.timeline().window_secs(),
            app.session.timeline().follow_offset_secs(),
        );
        let timeline = app.session.timeline_mut();
        ui.horizontal_wrapped(|ui| {
            let mut window = timeline.window_secs();
            ui.label("Window:");
            if ui
                .add(
                    egui::DragValue::new(&mut window)
                        .range(0.0..=f64::MAX)
                        .suffix(" s"),
                )
                .changed()
                && window.is_finite()
            {
                timeline.set_window_secs(window);
            }
            let mut offset = timeline.follow_offset_secs();
            ui.label("Follow offset:");
            if ui
                .add(egui::DragValue::new(&mut offset).suffix(" s"))
                .changed()
                && offset.is_finite()
            {
                timeline.set_follow_offset_secs(offset);
            }
            if ui.button("Pause").clicked() {
                timeline.set_setpoint(timeline.setpoint());
            }
            if ui.button("Go live").clicked() {
                timeline.go_live();
            }
        });
        for field in 0..3 {
            let (name, time, track) = match field {
                0 => ("Start", timeline.start(), timeline.start_track()),
                1 => ("End", timeline.end(), timeline.end_track()),
                _ => ("Setpoint", timeline.setpoint(), timeline.setpoint_track()),
            };
            ui.horizontal(|ui| {
                ui.label(name);
                ui.label(time.label());
                let mut ms = time.unix_millis();
                if ui
                    .add(
                        egui::DragValue::new(&mut ms)
                            .speed(100.0)
                            .suffix(" Unix ms"),
                    )
                    .changed()
                {
                    let t = Time::from_unix_millis(ms);
                    match field {
                        0 => timeline.set_start(t),
                        1 => timeline.set_end(t),
                        _ => timeline.set_setpoint(t),
                    }
                }
                let mut marching = track == Track::Marching;
                if ui.checkbox(&mut marching, "Marching").changed() {
                    match (field, marching) {
                        (0, true) => timeline.release_start(),
                        (1, true) => timeline.release_end(),
                        (_, true) => timeline.release_setpoint(),
                        (0, false) => timeline.set_start(time),
                        (1, false) => timeline.set_end(time),
                        _ => timeline.set_setpoint(time),
                    }
                }
            });
        }
        let mut cursor = timeline.setpoint().unix_millis();
        if ui
            .add(
                egui::Slider::new(
                    &mut cursor,
                    timeline.start().unix_millis()..=timeline.end().unix_millis(),
                )
                .text("Scrub")
                .show_value(false),
            )
            .changed()
        {
            timeline.set_setpoint(Time::from_unix_millis(cursor));
        }
        let changed = before != (timeline.window_secs(), timeline.follow_offset_secs());
        if ui.button("Clear shared history").clicked() {
            app.session.reset(Time::now());
            app.bus_load_samples.clear();
        }
        if let Some((start, end)) = app.session.cache().time_span() {
            ui.label(format!(
                "Retained: {} – {} ({} frames)",
                start.label(),
                end.label(),
                app.session.cache().len()
            ));
        } else {
            ui.label("No retained history.");
        }
        if let Some(error) = &app.diagnostic {
            ui.colored_label(ui.visuals().error_fg_color, error);
        }
        if changed {
            app.save_settings();
        }
    });
}
