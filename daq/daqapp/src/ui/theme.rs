use eframe::egui;

const NORD_THEME_PATH: &str = "themes/nord.toml";
const CATPPUCCIN_THEME_PATH: &str = "themes/catppuccin.toml";
const ONEDARK_THEME_PATH: &str = "themes/onedark.toml";

#[derive(Copy, Clone, serde::Serialize, serde::Deserialize, Debug)]
pub enum ThemeSelection {
    Default,
    Nord,
    Catppuccin,
    OneDark,
}

impl ThemeSelection {
    pub fn get_name(&self) -> &'static str {
        match self {
            ThemeSelection::Default => "Default",
            ThemeSelection::Nord => "Nord",
            ThemeSelection::Catppuccin => "Catppuccin",
            ThemeSelection::OneDark => "One Dark",
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
            ThemeSelection::OneDark => ThemeColors::load_from_file(ONEDARK_THEME_PATH)
                .map(|t| t.to_egui_style())
                .unwrap_or_default(),
        }
    }

    /// Load the ThemeColors for this selection (for storing in ctx).
    pub fn get_colors(&self) -> ThemeColors {
        let path = match self {
            ThemeSelection::Nord => Some(NORD_THEME_PATH),
            ThemeSelection::Catppuccin => Some(CATPPUCCIN_THEME_PATH),
            ThemeSelection::OneDark => Some(ONEDARK_THEME_PATH),
            ThemeSelection::Default => None,
        };
        path.and_then(ThemeColors::load_from_file)
            .unwrap_or_default()
    }

    pub fn next(&self) -> Self {
        match self {
            ThemeSelection::Default => ThemeSelection::Nord,
            ThemeSelection::Nord => ThemeSelection::Catppuccin,
            ThemeSelection::Catppuccin => ThemeSelection::OneDark,
            ThemeSelection::OneDark => ThemeSelection::Default,
        }
    }
}

/// Store the current ThemeColors into egui's context so any widget can read it.
/// Call this once whenever the user switches themes.
pub fn store_theme(ctx: &egui::Context, colors: ThemeColors) {
    ctx.data_mut(|d| d.insert_persisted(egui::Id::new("app_theme"), colors));
}

/// Read the current ThemeColors from egui's context.
/// Falls back to ThemeColors::default() if nothing has been stored yet.
pub fn get_theme(ctx: &egui::Context) -> ThemeColors {
    ctx.data_mut(|d| {
        d.get_persisted::<ThemeColors>(egui::Id::new("app_theme"))
            .unwrap_or_default()
    })
}

#[derive(Debug, Clone, serde::Serialize, serde::Deserialize)]
pub struct ThemeColors {
    pub background: String,
    pub panel_bg: String,
    pub text: String,
    pub accent: String,
    pub button: String,
    pub button_hover: String,
    pub button_text: String,
    // Semantic colors — optional so old TOMLs without them don't break
    #[serde(default)]
    pub error: Option<String>,
    #[serde(default)]
    pub warning: Option<String>,
    #[serde(default)]
    pub success: Option<String>,
    #[serde(default)]
    pub info: Option<String>,
}

impl Default for ThemeColors {
    fn default() -> Self {
        Self {
            background: "#1e1e1e".to_string(),
            panel_bg: "#252526".to_string(),
            text: "#d4d4d4".to_string(),
            accent: "#3c3c3c".to_string(),
            button: "#3c3c3c".to_string(),
            button_hover: "#505050".to_string(),
            button_text: "#cccccc".to_string(),
            error: Some("#f44747".to_string()),
            warning: Some("#ce9178".to_string()),
            success: Some("#6a9955".to_string()),
            info: Some("#569cd6".to_string()),
        }
    }
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
            _ => egui::Color32::from_rgb(255, 0, 255),
        }
    }

    // Convenience getters — use fallback colors if the TOML field was absent
    pub fn error_color(&self) -> egui::Color32 {
        self.error
            .as_deref()
            .map(Self::parse_hex)
            .unwrap_or(egui::Color32::from_rgb(224, 108, 117))
    }

    pub fn warning_color(&self) -> egui::Color32 {
        self.warning
            .as_deref()
            .map(Self::parse_hex)
            .unwrap_or(egui::Color32::from_rgb(209, 154, 102))
    }

    pub fn success_color(&self) -> egui::Color32 {
        self.success
            .as_deref()
            .map(Self::parse_hex)
            .unwrap_or(egui::Color32::from_rgb(152, 195, 121))
    }

    pub fn info_color(&self) -> egui::Color32 {
        self.info
            .as_deref()
            .map(Self::parse_hex)
            .unwrap_or(egui::Color32::from_rgb(97, 175, 239))
    }

    pub fn text_color(&self) -> egui::Color32 {
        Self::parse_hex(&self.text)
    }

    pub fn panel_color(&self) -> egui::Color32 {
        Self::parse_hex(&self.panel_bg)
    }

    pub fn accent_color(&self) -> egui::Color32 {
        Self::parse_hex(&self.accent)
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
