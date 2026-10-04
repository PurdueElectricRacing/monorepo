use crate::telemetry;

const DEFAULT_CENTER_LAT: f64 = 40.4344;
const DEFAULT_CENTER_LON: f64 = -86.9183;
// satellite imagery instead of street map
// <https://www.arcgis.com/home/item.html?id=10df2279f9684e4a9f6a7f08febac2a9>
struct EsriWorldImagery;

impl walkers::sources::TileSource for EsriWorldImagery {
    fn tile_url(&self, tile_id: walkers::TileId) -> String {
        format!(
            "https://server.arcgisonline.com/ArcGIS/rest/services/World_Imagery/MapServer/tile/{}/{}/{}",
            tile_id.zoom,
            tile_id.y,
            tile_id.x // ArcGIS wants zoom/row(y)/col(x), not zoom/x/y
        )
    }

    fn attribution(&self) -> walkers::sources::Attribution {
        walkers::sources::Attribution {
            text: "Esri, Maxar, Earthstar Geographics, and the GIS User Community",
            url: "https://www.esri.com/en-us/legal/copyright-trademarks",
            logo_light: None,
            logo_dark: None,
        }
    }
}

pub struct GpsPlot {
    pub title: String,
    tiles: Option<walkers::HttpTiles>,
    map_memory: walkers::MapMemory,
}

impl GpsPlot {
    pub fn new(instance: usize) -> Self {
        Self {
            title: format!("GPS Plot #{instance}"),
            tiles: None,
            map_memory: walkers::MapMemory::default(),
        }
    }

    pub fn show(
        &mut self,
        ui: &mut eframe::egui::Ui,
        view: &telemetry::TelemetryView<'_>,
    ) -> egui_tiles::UiResponse {
        let samples: Vec<_> = view.frames.iter().filter_map(gps_sample).collect();
        let fix = samples.last().copied();
        if let Some((t, lat, lon)) = fix {
            ui.label(format!("Last fix: {lat:.6}, {lon:.6} @ {}", t.label()));
        } else {
            ui.label("No retained GPS fix in the selected interval.");
        }

        let position = fix
            .map(|(_, lat, lon)| walkers::lon_lat(lon, lat))
            .unwrap_or_else(|| walkers::lon_lat(DEFAULT_CENTER_LON, DEFAULT_CENTER_LAT));
        if fix.is_some() && self.map_memory.detached().is_none() {
            self.map_memory.center_at(position);
        }

        let span = view.timeline.setpoint().secs(view.timeline.start());
        let stride = samples.len().div_ceil(4096).max(1);
        let mut trail: Vec<_> = samples
            .iter()
            .step_by(stride)
            .map(|(t, lat, lon)| {
                let age_fraction = if span > 0.0 {
                    (t.secs(view.timeline.start()) / span).clamp(0.0, 1.0) as f32
                } else {
                    1.0
                };

                let position = walkers::lon_lat(*lon, *lat);

                (position, age_fraction)
            })
            .collect();
        if let Some((_, lat, lon)) = fix {
            trail.push((walkers::lon_lat(lon, lat), 1.0));
        }

        let tiles = self
            .tiles
            .get_or_insert_with(|| walkers::HttpTiles::new(EsriWorldImagery, ui.ctx().clone()));
        ui.add(
            walkers::Map::new(Some(tiles), &mut self.map_memory, position).with_plugin(CarDot {
                trail,
                visible: fix.is_some(),
                color: eframe::egui::Color32::BLACK,
            }),
        );
        egui_tiles::UiResponse::None
    }
}
// drawing cars path
struct CarDot {
    trail: Vec<(walkers::Position, f32)>, // (position, age fraction 0.0=oldest..1.0=current)
    visible: bool,
    color: eframe::egui::Color32,
}

impl walkers::Plugin for CarDot {
    fn run(
        self: Box<Self>,
        ui: &mut eframe::egui::Ui,
        _response: &eframe::egui::Response,
        projector: &walkers::Projector,
        _map_memory: &walkers::MapMemory,
    ) {
        if !self.visible || self.trail.is_empty() {
            return;
        }

        let painter = ui.painter();
        let screen_points: Vec<eframe::egui::Pos2> = self
            .trail
            .iter()
            .map(|(position, _)| projector.project(*position).to_pos2())
            .collect();

        // drawing the trail as a bunch of connected lines
        for i in 1..screen_points.len() {
            // age of the segment's newer endpoint drives its look
            let (_, age) = self.trail[i];
            let fade = 0.15 + 0.85 * age;
            let width = 1.0 + 2.5 * age;

            painter.line_segment(
                [screen_points[i - 1], screen_points[i]],
                eframe::egui::Stroke::new(width, self.color.gamma_multiply(fade)),
            );
        }

        if let Some(&current) = screen_points.last() {
            // dot for current pos
            painter.circle_filled(current, 4.0, self.color);
            painter.circle_stroke(
                current,
                4.0,
                eframe::egui::Stroke::new(1.5_f32, eframe::egui::Color32::WHITE),
            );
        }
    }
}

fn gps_sample(f: &daqcore::ParsedFrame) -> Option<(daqcore::Time, f64, f64)> {
    let d = f.decoded.as_ref()?;
    if d.name != "gps_coordinates" {
        return None;
    }

    let lat = d.signals.get("latitude")?.value.physical;
    let lon = d.signals.get("longitude")?.value.physical;
    if !lat.is_finite() || !lon.is_finite() || lat.abs() > 90.0 || lon.abs() > 180.0 {
        return None;
    }
    Some((f.timestamp, lat, lon))
}
