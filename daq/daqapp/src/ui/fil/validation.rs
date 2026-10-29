use super::*;

impl FilControl {
    pub fn builder_board_name(board: &config::BuiltBoard) -> String {
        if !board.name.trim().is_empty() {
            return board.name.clone();
        }
        board
            .elf
            .file_stem()
            .map(|stem| stem.to_string_lossy().into_owned())
            .filter(|stem| !stem.is_empty())
            .unwrap_or_else(|| "board".into())
    }
    pub fn connect_issues(&self) -> Vec<String> {
        if self.settings.executable.is_none() {
            return vec!["Select a FIL executable first".into()];
        }
        if self.settings.use_builder {
            return self.builder_issues();
        }
        if self.settings.network.is_none() {
            return vec!["Select a network config first".into()];
        }
        if let Some(error) = &self.network_info_error {
            return vec![error.clone()];
        }
        let Some(info) = &self.network_info else {
            return vec!["Loading network config…".into()];
        };
        let mut issues = Vec::new();
        if self.enabled_file_boards().is_empty() {
            issues.push("Enable at least one board below".into());
        }
        for board in &info.boards {
            if self.settings.disabled_boards.contains(&board.name) {
                continue;
            }
            match config::effective_elf(board, &self.settings.elf_overrides) {
                Some(elf) if elf.is_file() => {}
                Some(elf) => issues.push(format!(
                    "Board '{}' ELF does not exist: {}",
                    board.name,
                    elf.display()
                )),
                None => issues.push(format!(
                    "Board '{}' has no firmware ELF; select one below",
                    board.name
                )),
            }
        }
        issues
    }
    pub fn builder_issues(&self) -> Vec<String> {
        let mut issues = Vec::new();
        if self.settings.builder.name.trim().is_empty() {
            issues.push("Give the built network a name".into());
        }
        if !matches!(self.settings.builder.bus.parse::<u8>(), Ok(0..=7)) {
            issues.push("Use a SuperDBC bus ID from 0 to 7 for the built network".into());
        }
        if self.settings.builder.bitrate == 0 {
            issues.push("Built network bitrate must be nonzero".into());
        }
        for stimulus in &self.settings.builder.stimuli {
            if !stimulus.is_file() {
                issues.push(format!(
                    "Stimulus script does not exist: {}",
                    stimulus.display()
                ));
            }
        }
        let enabled: Vec<&config::BuiltBoard> = self
            .settings
            .builder
            .boards
            .iter()
            .filter(|board| board.enabled)
            .collect();
        if enabled.is_empty() {
            issues.push("Add and enable at least one board below".into());
        }
        let mut names = HashSet::new();
        for board in enabled {
            if !board.board.as_os_str().is_empty() {
                if !board.board.is_file() {
                    issues.push(format!(
                        "Board config does not exist: {}",
                        board.board.display()
                    ));
                }
                continue;
            }
            let name = Self::builder_board_name(board);
            if board.name.trim().is_empty() && board.elf.as_os_str().is_empty() {
                issues.push("A built board needs a name and an ELF".into());
                continue;
            }
            if !names.insert(name.clone()) {
                issues.push(format!("Duplicate board name '{name}'"));
            }
            if !board.elf.is_file() {
                issues.push(format!(
                    "Board '{name}' ELF does not exist: {}",
                    board.elf.display()
                ));
            }
            match config::effective_mcu(board, self.settings.executable.as_deref()) {
                Some(mcu) if mcu.is_file() => {}
                Some(mcu) => issues.push(format!(
                    "Board '{name}' MCU config does not exist: {}",
                    mcu.display()
                )),
                None => issues.push(format!(
                    "Board '{name}' has no MCU config; pick one or point the FIL executable at a fil build"
                )),
            }
            let instances = if board.can_instances.is_empty() {
                vec!["FDCAN1"]
            } else {
                board.can_instances.iter().map(String::as_str).collect()
            };
            for instance in instances {
                if !config::FIL_CAN_INSTANCES.contains(&instance) {
                    issues.push(format!(
                        "Board '{name}' has unknown CAN instance '{instance}'"
                    ));
                }
            }
        }
        issues
    }
}

