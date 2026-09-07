use std::path::Path;
use std::process::Command;

pub fn is_root() -> bool {
    Command::new("su")
        .arg("-c")
        .arg("id")
        .status()
        .map(|s| s.success())
        .unwrap_or(false)
}

/// True when a usable rootless `proot` backend is deployed for the app.
///
/// `files_dir` is the app-private files directory
/// (`/data/data/<pkg>/files`); proot is expected at `<files_dir>/bin/proot`.
pub fn is_proot_available(files_dir: &str) -> bool {
    let bin = Path::new(files_dir).join("bin").join("proot");
    if !(bin.is_file()) {
        return false;
    }
    // Must be a non-empty ELF binary (assets deploy could have failed).
    match std::fs::metadata(&bin) {
        Ok(meta) => meta.len() > 4096,
        Err(_) => false,
    }
}
