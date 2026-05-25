use std::process::Command;

pub fn is_root() -> bool {
    Command::new("su")
        .arg("-c")
        .arg("id")
        .status()
        .map(|s| s.success())
        .unwrap_or(false)
}
