#[cfg(feature = "smithay_android")]
use std::sync::atomic::{AtomicBool, Ordering};
#[cfg(feature = "smithay_android")]
use std::time::{SystemTime, UNIX_EPOCH};

#[cfg(feature = "smithay_android")]
static RENDERING_ACTIVE: AtomicBool = AtomicBool::new(true);

/// Power Management: freeze or resume rendering.
/// Called from JNI when the Android activity goes to background (onStop) or foreground (onResume).
#[cfg(feature = "smithay_android")]
pub fn set_rendering_active(active: bool) {
    RENDERING_ACTIVE.store(active, Ordering::Relaxed);
    log::info!("SmithayRuntime: rendering_active = {}", active);
}

pub fn is_rendering_active() -> bool {
    RENDERING_ACTIVE.load(Ordering::Relaxed)
}

#[cfg(feature = "smithay_android")]
pub fn now_ms_u32() -> u32 {
    SystemTime::now()
        .duration_since(UNIX_EPOCH)
        .map(|d| d.as_millis() as u32)
        .unwrap_or(0)
}

#[cfg(feature = "smithay_android")]
fn now_millis() -> u128 {
    SystemTime::now()
        .duration_since(UNIX_EPOCH)
        .map(|duration| duration.as_millis())
        .unwrap_or(0)
}

#[cfg(feature = "smithay_android")]
pub(crate) fn append_runtime_trace(socket_dir: &std::path::Path, message: &str) {
    if std::fs::create_dir_all(socket_dir).is_err() {
        return;
    }

    let trace_file = socket_dir.join("wayland-native-trace.txt");
    if let Ok(mut file) = std::fs::OpenOptions::new()
        .create(true)
        .append(true)
        .open(trace_file)
    {
        use std::io::Write;
        let _ = writeln!(file, "{} {}", now_millis(), message);
    }
}

#[cfg(feature = "smithay_android")]
pub fn emit_hybrid_trace(message: String) {
    // Keep this independent from logger setup order issues.
    eprintln!("{}", message);
    log::debug!("{}", message);
}

#[cfg(feature = "smithay_android")]
#[allow(dead_code)]
pub(crate) fn rect_contains_f32(rect: &(f32, f32, f32, f32), x: f32, y: f32) -> bool {
    let (rx, ry, rw, rh) = *rect;
    x >= rx && x < rx + rw && y >= ry && y < ry + rh
}

#[cfg(feature = "smithay_android")]
pub(crate) fn touch_slot_from_id(id: i32) -> smithay::backend::input::TouchSlot {
    if id >= 0 {
        smithay::backend::input::TouchSlot::from(Some(id as u32))
    } else {
        smithay::backend::input::TouchSlot::from(None)
    }
}

#[cfg(feature = "smithay_android")]
pub(crate) fn point_from_xy(x: f32, y: f32) -> smithay::utils::Point<f64, smithay::utils::Logical> {
    (x as f64, y as f64).into()
}

#[cfg(feature = "smithay_android")]
pub fn char_to_xkb_scancode(ch: char) -> Option<(u32, bool)> {
    let (evdev, shift) = match ch {
        'a'..='z' => {
            let table = [
                30, 48, 46, 32, 18, 33, 34, 35, 23, 36, 37, 38, 50, 49, 24, 25, 16, 19, 31, 20, 22,
                47, 17, 45, 21, 44,
            ];
            let idx = (ch as u8 - b'a') as usize;
            (table[idx], false)
        }
        'A'..='Z' => {
            let table = [
                30, 48, 46, 32, 18, 33, 34, 35, 23, 36, 37, 38, 50, 49, 24, 25, 16, 19, 31, 20, 22,
                47, 17, 45, 21, 44,
            ];
            let idx = (ch as u8 - b'A') as usize;
            (table[idx], true)
        }
        '1' => (2, false),
        '2' => (3, false),
        '3' => (4, false),
        '4' => (5, false),
        '5' => (6, false),
        '6' => (7, false),
        '7' => (8, false),
        '8' => (9, false),
        '9' => (10, false),
        '0' => (11, false),
        '!' => (2, true),
        '@' => (3, true),
        '#' => (4, true),
        '$' => (5, true),
        '%' => (6, true),
        '^' => (7, true),
        '&' => (8, true),
        '*' => (9, true),
        '(' => (10, true),
        ')' => (11, true),
        ' ' => (57, false),
        '\n' => (28, false),
        '\t' => (15, false),
        '\u{0008}' => (14, false),
        '-' => (12, false),
        '_' => (12, true),
        '=' => (13, false),
        '+' => (13, true),
        '[' => (26, false),
        '{' => (26, true),
        ']' => (27, false),
        '}' => (27, true),
        '\\' => (43, false),
        '|' => (43, true),
        ';' => (39, false),
        ':' => (39, true),
        '\'' => (40, false),
        '"' => (40, true),
        ',' => (51, false),
        '<' => (51, true),
        '.' => (52, false),
        '>' => (52, true),
        '/' => (53, false),
        '?' => (53, true),
        '`' => (41, false),
        '~' => (41, true),
        _ => return None,
    };

    Some((evdev + 8, shift))
}
