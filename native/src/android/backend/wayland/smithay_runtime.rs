// ── Smithay Runtime — Facade / Re-exports ──
// This file is auto-split into operation-specific modules.
// See individual modules for implementation details.

#[cfg(feature = "smithay_android")]
pub use crate::android::backend::wayland::seat::AndroidSeatRuntime;

#[cfg(feature = "smithay_android")]
pub use crate::android::backend::wayland::server::WaylandServer;

#[cfg(feature = "smithay_android")]
pub use crate::android::backend::wayland::server::configure_xkb;

#[cfg(feature = "smithay_android")]
pub use crate::android::backend::wayland::input_router::{
    XkbContext, XkbKeymap, XkbKeymapCompileFlags,
};
