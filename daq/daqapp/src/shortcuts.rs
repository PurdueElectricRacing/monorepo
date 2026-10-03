use crate::action;

pub struct ShortcutHandler;

impl ShortcutHandler {
    pub fn check_shortcuts(ctx: &eframe::egui::Context) -> Vec<action::AppAction> {
        let mut actions = Vec::new();

        // Get input state
        let input = ctx.input(|i| i.clone());

        // CMD+S = toggle sidebar
        if input.modifiers.command_only() && input.key_pressed(eframe::egui::Key::S) {
            actions.push(action::AppAction::ToggleSidebar);
        }

        // CMD+W = close window
        if input.modifiers.command_only() && input.key_pressed(eframe::egui::Key::W) {
            actions.push(action::AppAction::CloseActiveWidget);
        }

        // CMD+P = command palette
        if input.modifiers.command_only() && input.key_pressed(eframe::egui::Key::P) {
            actions.push(action::AppAction::ToggleCommandPalette);
        }

        // CMD+Plus = increase scale
        if input.modifiers.command_only() && input.key_pressed(eframe::egui::Key::Equals) {
            actions.push(action::AppAction::IncreaseScale);
        }

        // CMD+Minus = decrease scale
        if input.modifiers.command_only() && input.key_pressed(eframe::egui::Key::Minus) {
            actions.push(action::AppAction::DecreaseScale);
        }

        actions
    }
}
