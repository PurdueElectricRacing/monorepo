use crate::messages;
use chrono::{DateTime, Local, Timelike};
use eframe::egui;
use std::collections::VecDeque;
use walkers::sources::{Attribution, TileSource};
use walkers::{HttpTiles, Map, MapMemory, Plugin, Position, Projector, TileId, lon_lat};

// default starting is ross ade
const DEFAULT_CENTER_LAT: f64 = 40.4344;
const DEFAULT_CENTER_LON: f64 = -86.9183;

// default length of trail behind dot, in seconds
const DEFAULT_TRAIL_SECONDS: f64 = 2.0;
const MIN_TRAIL_SECONDS: f64 = 0.5; // slider lower bound
const MAX_TRAIL_SECONDS: f64 = 20.0; // slider upper bound

// satellite imagery instead of street map
// <https://www.arcgis.com/home/item.html?id=10df2279f9684e4a9f6a7f08febac2a9>
struct EsriWorldImagery;

impl TileSource for EsriWorldImagery {
    fn tile_url(&self, tile_id: TileId) -> String {
        format!(
            "https://server.arcgisonline.com/ArcGIS/rest/services/World_Imagery/MapServer/tile/{}/{}/{}",
            tile_id.zoom,
            tile_id.y,
            tile_id.x // ArcGIS wants zoom/row(y)/col(x), not zoom/x/y
        )
    }

    fn attribution(&self) -> Attribution {
        Attribution {
            text: "Esri, Maxar, Earthstar Geographics, and the GIS User Community",
            url: "https://www.esri.com/en-us/legal/copyright-trademarks",
            logo_light: None,
            logo_dark: None,
        }
    }
}

pub struct GpsPlot {
    pub title: String,
    tiles: Option<HttpTiles>,
    map_memory: MapMemory,
    current_fix: Option<(DateTime<Local>, f64, f64)>, // (timestamp, lat, lon)
    trail: VecDeque<(DateTime<Local>, Position)>,     // (timestamp, position), oldest first
    trail_seconds: f64,                               // current trail duration, set by the slider
}

impl GpsPlot {
    pub fn new(instance_num: usize) -> Self {
        Self {
            title: format!("GPS Plot #{}", instance_num), // set widget title
            tiles: None,                                  // map tiles created later in show
            map_memory: MapMemory::default(),             // initialize map state
            current_fix: None,                            // no gps data yet
            trail: VecDeque::new(),                       // start with empty trail
            trail_seconds: DEFAULT_TRAIL_SECONDS,         // start at default trail duration
        }
    }

    fn extract_sample(msg: &messages::MsgFromCan) -> Option<(DateTime<Local>, f64, f64)> {
        let messages::MsgFromCan::ParsedMessage(parsed) = msg else {
            return None; // ignore if msg isn't a parsed can message
        };

        if parsed.decoded.name != "gps_coordinates" {
            return None; // ignore non gps messages
        }

        let mut lat = None;
        let mut lon = None;

        for (_, sig) in &parsed.decoded.signals {
            // loop through all gps signals
            match sig.name.as_str() {
                "longitude" => lon = Some(sig.value.physical), // save long
                "latitude" => lat = Some(sig.value.physical),  // save lat
                _ => {}                                        // ignore other signals
            }
        }

        match (lat, lon) {
            (Some(lat), Some(lon)) => Some((parsed.timestamp, lat, lon)), // return GPS data
            _ => None,                                                    // missing lat or lon
        }
    }

    // drops any trail points older than trail_seconds
    fn prune_trail(&mut self) {
        let Some(newest) = self.trail.back().map(|(ts, _)| *ts) else {
            return; // nothing to prune
        };

        while let Some((oldest, _)) = self.trail.front() {
            let age_secs = (newest - *oldest).num_milliseconds() as f64 / 1000.0;
            if age_secs > self.trail_seconds {
                self.trail.pop_front();
            } else {
                break;
            }
        }
    }

    pub fn handle_can_message(&mut self, msg: &messages::MsgFromCan) {
        if let Some(sample) = Self::extract_sample(msg) {
            // if msg is valid gps data sample is timestamp, lat, long
            let (timestamp, lat, lon) = sample; // pulls out lat and long
            self.current_fix = Some(sample); // replaces old pos w new pos

            self.trail.push_back((timestamp, lon_lat(lon, lat))); // add point to trail
            self.prune_trail(); // drop anything older than trail_seconds
        }
    }

    pub fn show(&mut self, ui: &mut egui::Ui) -> egui_tiles::UiResponse {
        ui.horizontal(|ui| {
            // slider row up top
            ui.label("Trail length:");
            ui.add(
                egui::Slider::new(
                    &mut self.trail_seconds,
                    MIN_TRAIL_SECONDS..=MAX_TRAIL_SECONDS,
                )
                .suffix(" s")
                .step_by(0.1),
            );
        });

        self.prune_trail(); // trim right away if slider got dragged down

        match self.current_fix {
            Some((timestamp, lat, lon)) => {
                // if gps data exists show it
                ui.label(format!(
                    "Last fix: {lat:.6}, {lon:.6}  @ {:02}:{:02}:{:02}.{}",
                    timestamp.hour(),
                    timestamp.minute(),
                    timestamp.second(),
                    timestamp.timestamp_subsec_millis() / 100
                ));
            }
            None => {
                ui.label("Waiting for GPS_Position CAN message..."); // no gps data yet
            }
        }

        ui.add_space(4.0); // add spacing

        let has_fix = self.current_fix.is_some(); // check if gps data exists

        let car_position = self
            .current_fix
            .map(|(_, lat, lon)| lon_lat(lon, lat)) // use current gps position
            .unwrap_or_else(|| lon_lat(DEFAULT_CENTER_LON, DEFAULT_CENTER_LAT)); // otherwise use default position

        // centering map on car unless user moves
        if has_fix && self.map_memory.detached().is_none() {
            self.map_memory.center_at(car_position);
        }

        // work out how far into the trail's time window each point falls,
        // 0.0 = oldest end of the window, 1.0 = current position
        let oldest_ts = self.trail.front().map(|(ts, _)| *ts);
        let newest_ts = self.trail.back().map(|(ts, _)| *ts);
        let aged_trail: Vec<(Position, f32)> = match (oldest_ts, newest_ts) {
            (Some(oldest), Some(newest)) => {
                let span_secs = (newest - oldest).num_milliseconds() as f64 / 1000.0;
                self.trail
                    .iter()
                    .map(|(ts, pos)| {
                        let age = if span_secs > 0.0 {
                            ((*ts - oldest).num_milliseconds() as f64 / 1000.0 / span_secs) as f32
                        } else {
                            1.0 // only one point in the trail, treat it as "current"
                        };
                        (*pos, age)
                    })
                    .collect()
            }
            _ => Vec::new(), // no points yet
        };

        let tiles = self
            .tiles
            .get_or_insert_with(|| HttpTiles::new(EsriWorldImagery, ui.ctx().clone())); // create map tiles if they don't exist, right before we need them

        ui.add(
            Map::new(Some(tiles), &mut self.map_memory, car_position).with_plugin(CarDot {
                trail: aged_trail,           // pass trail (with age) to plugin
                visible: has_fix,            // only draw if gps exists
                color: egui::Color32::BLACK, // draw in black
            }),
        );

        egui_tiles::UiResponse::None // nothing else to return
    }
}

// drawing cars path
struct CarDot {
    trail: Vec<(Position, f32)>, // (position, age fraction 0.0=oldest..1.0=current)
    visible: bool,
    color: egui::Color32,
}

impl Plugin for CarDot {
    fn run(
        self: Box<Self>,
        ui: &mut egui::Ui,
        _response: &egui::Response,
        projector: &Projector,
        _map_memory: &MapMemory,
    ) {
        if !self.visible || self.trail.is_empty() {
            return;
        }

        let painter = ui.painter();
        let screen_points: Vec<egui::Pos2> = self
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
                egui::Stroke::new(width, self.color.gamma_multiply(fade)),
            );
        }

        if let Some(&current) = screen_points.last() {
            // dot for current pos
            painter.circle_filled(current, 4.0, self.color);
            painter.circle_stroke(current, 4.0, egui::Stroke::new(1.5, egui::Color32::WHITE));
        }
    }
}
