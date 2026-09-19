use crate::action::AppAction;
use eframe::egui;

pub struct ShortcutHandler;

impl ShortcutHandler {
    pub fn check_shortcuts(ctx: &egui::Context) -> Vec<AppAction> {
        let mut actions = Vec::new();

        // Get input state
        let input = ctx.input(|i| i.clone());

        // CMD+S = toggle sidebar
        if input.modifiers.command_only() && input.key_pressed(egui::Key::S) {
            actions.push(AppAction::ToggleSidebar);
        }

        // CMD+W = close window
        if input.modifiers.command_only() && input.key_pressed(egui::Key::W) {
            actions.push(AppAction::CloseActiveWidget);
        }

        // CMD+P = command palette
        if input.modifiers.command_only() && input.key_pressed(egui::Key::P) {
            actions.push(AppAction::ToggleCommandPalette);
        }

        // CMD+Plus = increase scale
        if input.modifiers.command_only() && input.key_pressed(egui::Key::Equals) {
            actions.push(AppAction::IncreaseScale);
        }

        // CMD+Minus = decrease scale
        if input.modifiers.command_only() && input.key_pressed(egui::Key::Minus) {
            actions.push(AppAction::DecreaseScale);
        }

        actions
    }
}
