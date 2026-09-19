use eframe::egui;

const NORD_THEME_PATH: &str = "themes/nord.toml";
const CATPPUCCIN_THEME_PATH: &str = "themes/catppuccin.toml";

#[derive(Copy, Clone, serde::Serialize, serde::Deserialize, Debug)]
pub enum ThemeSelection {
    Default,
    Nord,
    Catppuccin,
}

impl ThemeSelection {
    pub fn get_name(&self) -> &'static str {
        match self {
            ThemeSelection::Default => "Default",
            ThemeSelection::Nord => "Nord",
            ThemeSelection::Catppuccin => "Catppuccin",
        }
    }

    pub fn get_style(&self) -> egui::Style {
        match self {
            ThemeSelection::Default => egui::Style::default(),
            ThemeSelection::Nord => ThemeColors::load_from_file(NORD_THEME_PATH)
                .map(|t| t.to_egui_style())
                .unwrap_or_default(),
            ThemeSelection::Catppuccin => ThemeColors::load_from_file(CATPPUCCIN_THEME_PATH)
                .map(|t| t.to_egui_style())
                .unwrap_or_default(),
        }
    }

    pub fn next(&self) -> Self {
        match self {
            ThemeSelection::Default => ThemeSelection::Nord,
            ThemeSelection::Nord => ThemeSelection::Catppuccin,
            ThemeSelection::Catppuccin => ThemeSelection::Default,
        }
    }
}

#[derive(Debug, serde::Deserialize, Clone)]
pub struct ThemeColors {
    pub background: String,
    pub panel_bg: String,
    pub text: String,
    pub accent: String,
    pub button: String,
    pub button_hover: String,
    pub button_text: String,
}

impl ThemeColors {
    pub fn parse_hex(hex: &str) -> egui::Color32 {
        let hex = hex.trim_start_matches('#');

        match hex.len() {
            6 => {
                let r = u8::from_str_radix(&hex[0..2], 16).unwrap_or(0);
                let g = u8::from_str_radix(&hex[2..4], 16).unwrap_or(0);
                let b = u8::from_str_radix(&hex[4..6], 16).unwrap_or(0);
                egui::Color32::from_rgb(r, g, b)
            }
            8 => {
                let r = u8::from_str_radix(&hex[0..2], 16).unwrap_or(0);
                let g = u8::from_str_radix(&hex[2..4], 16).unwrap_or(0);
                let b = u8::from_str_radix(&hex[4..6], 16).unwrap_or(0);
                let a = u8::from_str_radix(&hex[6..8], 16).unwrap_or(255);
                egui::Color32::from_rgba_unmultiplied(r, g, b, a)
            }
            _ => egui::Color32::from_rgb(255, 0, 255), // fallback magenta for invalid
        }
    }

    pub fn to_egui_style(&self) -> egui::Style {
        let mut style = egui::Style::default();

        let bg = Self::parse_hex(&self.background);
        let panel = Self::parse_hex(&self.panel_bg);
        let text = Self::parse_hex(&self.text);
        let accent = Self::parse_hex(&self.accent);

        let button = Self::parse_hex(&self.button);
        let button_hover = Self::parse_hex(&self.button_hover);
        let button_text = Self::parse_hex(&self.button_text);

        // --- Base ---
        style.visuals.window_fill = bg;
        style.visuals.panel_fill = panel;
        style.visuals.faint_bg_color = panel;
        style.visuals.override_text_color = Some(text);
        style.visuals.selection.bg_fill = bg;

        // --- Global button visuals ---
        style.visuals.widgets.inactive.weak_bg_fill = button;
        style.visuals.widgets.hovered.weak_bg_fill = button_hover;
        style.visuals.widgets.active.weak_bg_fill = button_hover;

        style.visuals.text_edit_bg_color = Some(button);

        style.visuals.widgets.inactive.fg_stroke.color = button_text;
        style.visuals.widgets.hovered.fg_stroke.color = button_text;
        style.visuals.widgets.active.fg_stroke.color = button_text;

        // Optional: slightly darker borders for contrast
        style.visuals.widgets.noninteractive.bg_stroke.color = accent.linear_multiply(0.3);

        style
    }

    pub fn load_from_file(path: &str) -> Option<Self> {
        std::fs::read_to_string(path)
            .ok()
            .and_then(|data| toml::from_str::<Self>(&data).ok())
    }
}
