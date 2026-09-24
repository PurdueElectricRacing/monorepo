use std::path::{Path, PathBuf};

fn find_in_directory<F>(directory: &Path, relative_path: &Path, matches: F) -> Option<PathBuf>
where
    F: Fn(&Path) -> bool,
{
    let candidate = directory.join(relative_path);
    matches(&candidate).then_some(candidate)
}

pub fn find_path<F>(relative_path: impl AsRef<Path>, matches: F) -> Option<PathBuf>
where
    F: Fn(&Path) -> bool,
{
    let current_directory = std::env::current_dir().ok()?;
    let relative_path = relative_path.as_ref();

    // Check launch-local overrides before the DaqApp package path from either repo root.
    for package_path in [Path::new("."), Path::new("daq/daqapp"), Path::new("daqapp")] {
        if let Some(path) = find_in_directory(
            &current_directory.join(package_path),
            relative_path,
            &matches,
        ) {
            return Some(path);
        }
    }

    None
}

pub fn find_file(relative_path: impl AsRef<Path>) -> Option<PathBuf> {
    find_path(relative_path, Path::is_file)
}

pub fn read_file(relative_path: impl AsRef<Path>) -> Option<String> {
    find_file(relative_path).and_then(|path| std::fs::read_to_string(path).ok())
}
