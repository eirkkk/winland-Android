pub mod backend;
pub mod utils;
pub mod command_channel;

// ── JNI Bridge — مقسم تلقائياً حسب الوظيفة ──
pub mod bridge_init;        // التهيئة ودورة الحياة (initWaylandConnection, ensureSocketRuntime, releaseWaylandConnection)
pub mod bridge_input;       // الإدخال (sendTouchEvent, sendKeyEvent, sendTextInput, setScrollSensitivity)
pub mod bridge_surface;     // السطح والشاشة (onSurfaceChanged, setRefreshRate, setResolution)
pub mod bridge_diagnostics; // التشخيص والأخطاء (getLastNativeError, getWaylandRuntimeStats)
pub mod bridge_clipboard;   // الحافظة (updateClipboard, sendClipboardTextToWayland)
pub mod bridge_lifecycle;   // دورة حياة العرض (suspendRendering, resumeRendering)
