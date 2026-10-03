use crate::app;

pub fn show(app: &mut app::DAQApp, ctx: &eframe::egui::Context) {
    eframe::egui::TopBottomPanel::top("shared_timeline").show(ctx, |ui| {
        let captured = app.session.cache().time_span();
        let before = app.session.timeline().window_secs();
        let timeline = app.session.timeline_mut();
        ui.horizontal_wrapped(|ui| {
            if ui.button("Go live").clicked() { timeline.go_live(); }
            ui.separator();
            let mut span = timeline.window_secs();
            ui.label("Live span:").on_hover_text("When start marches, it stays this far behind end. Drag a boundary to choose a new span.");
            if ui.add(eframe::egui::DragValue::new(&mut span).range(0.0..=f64::MAX).suffix(" s")).changed() && span.is_finite() {
                timeline.set_window_secs(span);
            }
            for field in 0..3 {
                let (_,time,track)=field_value(timeline,field);
                let mut marching=track==daqcore::timeline::Track::Marching;
                if ui.checkbox(&mut marching,match field { 0 => "Start follows end", 1 => "End follows capture", _ => "Playhead follows end" }).changed() {
                    match (field,marching) {
                        (0,true)=>timeline.release_start(), (1,true)=>timeline.release_end(), (_,true)=>timeline.release_setpoint(),
                        (0,false)=>timeline.set_start(time), (1,false)=>timeline.set_end(time), (_,false)=>timeline.set_setpoint(time),
                    }
                }
            }
        });
        let left=timeline.start().min(captured.map_or(timeline.start(),|(start,_)|start));
        let right=timeline.end().max(captured.map_or(timeline.end(),|(_,end)|end));
        ruler(ui,timeline,left,right);
        ui.horizontal_wrapped(|ui| {
            ui.label(format!("Selected: {} – {}",timeline.start().label(),timeline.end().label()));
            ui.label(format!("Playhead: {}",timeline.setpoint().label()));
        });
        let changed=before!=timeline.window_secs();
        if let Some((_,end))=captured {
            ui.label(format!("Captured through {} · {} frames retained",end.label(),app.session.cache().len()));
        } else { ui.label("No captured frames."); }
        if ui.button("Clear history").clicked() {
            app.session.reset(daqcore::Time::now()); app.bus_load_samples.clear();
        }
        if let Some(error)=&app.diagnostic { ui.colored_label(ui.visuals().error_fg_color,error); }
        if changed { app.save_settings(); }
    });
}

fn field_value(
    timeline: &daqcore::timeline::Timeline,
    field: usize,
) -> (&'static str, daqcore::Time, daqcore::timeline::Track) {
    match field {
        0 => ("start", timeline.start(), timeline.start_track()),
        1 => ("end", timeline.end(), timeline.end_track()),
        _ => ("playhead", timeline.setpoint(), timeline.setpoint_track()),
    }
}
fn ruler(
    ui: &mut eframe::egui::Ui,
    timeline: &mut daqcore::timeline::Timeline,
    left: daqcore::Time,
    right: daqcore::Time,
) {
    let (rect, background) = ui.allocate_exact_size(
        eframe::egui::vec2(ui.available_width(), 90.0),
        eframe::egui::Sense::click(),
    );
    let area = rect.shrink2(eframe::egui::vec2(42.0, 0.0));
    let span_ms = (right.unix_millis() as i128 - left.unix_millis() as i128).max(1) as f64;
    let x = |time: daqcore::Time| {
        area.left() + (time.secs(left) * 1000.0 / span_ms) as f32 * area.width()
    };
    let time = |px: f32| {
        left.offset(
            (((px - area.left()) / area.width()).clamp(0.0, 1.0) as f64 * span_ms).round() as i64,
        )
    };
    let axis = area.bottom() - 20.0;
    let selected = eframe::egui::Rect::from_min_max(
        eframe::egui::pos2(x(timeline.start()), area.top() + 8.0),
        eframe::egui::pos2(x(timeline.end()), axis),
    );
    ui.painter().rect_filled(
        selected,
        2.0,
        ui.visuals().selection.bg_fill.linear_multiply(0.3),
    );
    ui.painter().line_segment(
        [
            eframe::egui::pos2(area.left(), axis),
            eframe::egui::pos2(area.right(), axis),
        ],
        ui.visuals().widgets.noninteractive.fg_stroke,
    );
    for i in 0..=5 {
        let tick = left.offset((span_ms * i as f64 / 5.0).round() as i64);
        let px = x(tick);
        ui.painter().text(
            eframe::egui::pos2(px, axis + 4.0),
            eframe::egui::Align2::CENTER_TOP,
            tick.label(),
            eframe::egui::FontId::monospace(10.0),
            ui.visuals().text_color(),
        );
    }
    let mut dragged = false;
    for field in 0..3 {
        let (name, value, _) = field_value(timeline, field);
        let px = x(value);
        let y = area.top() + 10.0 + field as f32 * 19.0;
        let handle = eframe::egui::Rect::from_center_size(
            eframe::egui::pos2(px, y),
            eframe::egui::vec2(22.0, 18.0),
        );
        let response = ui
            .interact(
                handle,
                ui.id().with(("timeline_marker", field)),
                eframe::egui::Sense::drag(),
            )
            .on_hover_text(format!("Drag {name}: {}", value.label()));
        let color = if field == 2 {
            eframe::egui::Color32::YELLOW
        } else {
            ui.visuals().selection.stroke.color
        };
        ui.painter().line_segment(
            [eframe::egui::pos2(px, y), eframe::egui::pos2(px, axis)],
            eframe::egui::Stroke::new(1.0, color),
        );
        ui.painter()
            .circle_filled(eframe::egui::pos2(px, y), 4.0, color);
        let anchor = if field != 0 {
            eframe::egui::Align2::RIGHT_CENTER
        } else {
            eframe::egui::Align2::LEFT_CENTER
        };
        let dx = if field != 0 { -7.0 } else { 7.0 };
        ui.painter().text(
            eframe::egui::pos2(px + dx, y),
            anchor,
            name,
            eframe::egui::FontId::proportional(11.0),
            color,
        );
        if response.dragged()
            && let Some(pointer) = response.interact_pointer_pos()
        {
            let value = time(pointer.x);
            match field {
                0 => timeline.set_start(value),
                1 => timeline.set_end(value),
                _ => timeline.set_setpoint(value),
            }
            if field != 2 {
                timeline.set_window_secs(timeline.end().secs(timeline.start()));
            }
            dragged = true;
        }
    }
    if !dragged
        && background.clicked()
        && let Some(pointer) = background.interact_pointer_pos()
    {
        timeline.set_setpoint(time(pointer.x));
    }
}
