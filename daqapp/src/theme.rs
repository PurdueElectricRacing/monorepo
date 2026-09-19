use eframe::egui;

const NORD_THEME_PATH: &str = "themes/nord.toml";
const CATPPUCCIN_THEME_PATH: &str = "themes/catppuccin.toml";
const ONEDARK_THEME_PATH: &str = "themes/onedark.toml";

const NORD_THEME_SOURCE: &str = include_str!("../themes/nord.toml");
const CATPPUCCIN_THEME_SOURCE: &str = include_str!("../themes/catppuccin.toml");
const ONEDARK_THEME_SOURCE: &str = include_str!("../themes/onedark.toml");

#[derive(Copy, Clone, serde::Serialize, serde::Deserialize, Debug, PartialEq, Eq)]
pub enum ThemeSelection {
    Default,
    Nord,
    Catppuccin,
    OneDark,
}

impl Default for ThemeSelection {
    fn default() -> Self {
        Self::Default
    }
}

impl ThemeSelection {
    pub fn get_name(&self) -> &'static str {
        match self {
            Self::Default => "Default",
            Self::Nord => "Nord",
            Self::Catppuccin => "Catppuccin",
            Self::OneDark => "One Dark",
        }
    }

    pub fn get_style(&self) -> egui::Style {
        match self {
            Self::Default => egui::Style::default(),
            _ => self.get_colors().to_egui_style(),
        }
    }

    /// Load the colors for this selection for widgets that draw custom content.
    pub fn get_colors(&self) -> ThemeColors {
        match self {
            Self::Default => ThemeColors::default(),
            Self::Nord => load_builtin_theme(NORD_THEME_PATH, NORD_THEME_SOURCE),
            Self::Catppuccin => load_builtin_theme(CATPPUCCIN_THEME_PATH, CATPPUCCIN_THEME_SOURCE),
            Self::OneDark => load_builtin_theme(ONEDARK_THEME_PATH, ONEDARK_THEME_SOURCE),
        }
    }

    pub fn next(&self) -> Self {
        match self {
            Self::Default => Self::Nord,
            Self::Nord => Self::Catppuccin,
            Self::Catppuccin => Self::OneDark,
            Self::OneDark => Self::Default,
        }
    }
}

fn load_builtin_theme(path: &str, embedded_source: &str) -> ThemeColors {
    // Prefer the checked-in file so theme edits are picked up during development.
    // The embedded copy keeps packaged binaries working when the source tree is
    // not present beside the executable.
    let theme_path = std::path::Path::new(env!("CARGO_MANIFEST_DIR")).join(path);
    if theme_path.is_file() {
        if let Some(theme) = ThemeColors::load_from_file(&theme_path) {
            return theme;
        }
        log::warn!(
            "Failed to parse theme at {}; using embedded theme",
            theme_path.display()
        );
    }

    ThemeColors::from_toml(embedded_source).unwrap_or_else(|| {
        log::error!("Embedded theme {path} is invalid; using default colors");
        ThemeColors::default()
    })
}

/// Store the current colors in egui's context for custom widgets to use.
pub fn store_theme(ctx: &egui::Context, colors: ThemeColors) {
    ctx.data_mut(|data| data.insert_persisted(egui::Id::new("app_theme"), colors));
}

/// Read the colors currently used by custom widgets.
pub fn get_theme(ctx: &egui::Context) -> ThemeColors {
    ctx.data_mut(|data| {
        data.get_persisted::<ThemeColors>(egui::Id::new("app_theme"))
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
        let hex = hex.trim().trim_start_matches('#');
        let parse = |part: &str| u8::from_str_radix(part, 16).ok();

        let parsed = (|| -> Option<(u8, u8, u8, u8)> {
            match hex.len() {
                6 => Some((
                    parse(&hex[0..2])?,
                    parse(&hex[2..4])?,
                    parse(&hex[4..6])?,
                    255,
                )),
                8 => Some((
                    parse(&hex[0..2])?,
                    parse(&hex[2..4])?,
                    parse(&hex[4..6])?,
                    parse(&hex[6..8])?,
                )),
                _ => None,
            }
        })();

        let Some((r, g, b, a)) = parsed else {
            return egui::Color32::from_rgb(255, 0, 255);
        };

        egui::Color32::from_rgba_unmultiplied(r, g, b, a)
    }

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

        let background = Self::parse_hex(&self.background);
        let panel = Self::parse_hex(&self.panel_bg);
        let text = Self::parse_hex(&self.text);
        let accent = Self::parse_hex(&self.accent);
        let button = Self::parse_hex(&self.button);
        let button_hover = Self::parse_hex(&self.button_hover);
        let button_text = Self::parse_hex(&self.button_text);

        style.visuals.window_fill = background;
        style.visuals.panel_fill = panel;
        style.visuals.faint_bg_color = panel;
        style.visuals.extreme_bg_color = background;
        style.visuals.code_bg_color = panel;
        style.visuals.override_text_color = Some(text);
        style.visuals.hyperlink_color = accent;
        style.visuals.warn_fg_color = self.warning_color();
        style.visuals.error_fg_color = self.error_color();
        style.visuals.selection.bg_fill = accent;
        style.visuals.text_edit_bg_color = Some(background);
        style.visuals.window_stroke.color = accent.linear_multiply(0.5);

        style.visuals.widgets.noninteractive.bg_fill = panel;
        style.visuals.widgets.noninteractive.weak_bg_fill = panel;
        style.visuals.widgets.noninteractive.fg_stroke.color = text;
        style.visuals.widgets.noninteractive.bg_stroke.color = accent.linear_multiply(0.3);

        style.visuals.widgets.inactive.bg_fill = button;
        style.visuals.widgets.inactive.weak_bg_fill = button;
        style.visuals.widgets.inactive.fg_stroke.color = button_text;
        style.visuals.widgets.hovered.bg_fill = button_hover;
        style.visuals.widgets.hovered.weak_bg_fill = button_hover;
        style.visuals.widgets.hovered.fg_stroke.color = button_text;
        style.visuals.widgets.active.bg_fill = button_hover;
        style.visuals.widgets.active.weak_bg_fill = button_hover;
        style.visuals.widgets.active.fg_stroke.color = button_text;

        style
    }

    pub fn load_from_file(path: impl AsRef<std::path::Path>) -> Option<Self> {
        std::fs::read_to_string(path)
            .ok()
            .and_then(|data| Self::from_toml(&data))
    }

    fn from_toml(data: &str) -> Option<Self> {
        toml::from_str::<Self>(data).ok()
    }
}
