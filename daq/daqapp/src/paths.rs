use std::path::{Path, PathBuf};

fn find_from_launch_directory<F>(
    launch_directory: &Path,
    relative_path: &Path,
    matches: F,
) -> Option<PathBuf>
where
    F: Fn(&Path) -> bool,
{
    let candidate = launch_directory.join(relative_path);
    matches(&candidate).then_some(candidate)
}

pub fn find_path<F>(relative_path: impl AsRef<Path>, matches: F) -> Option<PathBuf>
where
    F: Fn(&Path) -> bool,
{
    let current_directory = std::env::current_dir().ok()?;

    // Resource paths are launch-directory-relative; run DaqApp from its package directory.
    find_from_launch_directory(&current_directory, relative_path.as_ref(), matches)
}

pub fn find_file(relative_path: impl AsRef<Path>) -> Option<PathBuf> {
    find_path(relative_path, Path::is_file)
}

pub fn read_file(relative_path: impl AsRef<Path>) -> Option<String> {
    find_file(relative_path).and_then(|path| std::fs::read_to_string(path).ok())
}
