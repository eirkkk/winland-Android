#[cfg(feature = "smithay_android")]
use std::sync::OnceLock;

#[cfg(feature = "smithay_android")]
use smithay::wayland::compositor::CompositorClientState;

#[cfg(feature = "smithay_android")]
use crate::android::backend::wayland::seat::AndroidSeatRuntime;

#[cfg(feature = "smithay_android")]
pub(crate) static FALLBACK_CLIENT_COMPOSITOR_STATE: OnceLock<CompositorClientState> = OnceLock::new();

// ── dispatch2 bridge ───────────────────────────────────────────────────────────
// Uses delegate_dispatch2! macro which generates blanket impls for both
// Dispatch<I, U> and GlobalDispatch<I, U> by delegating to the smithay
// Dispatch2/GlobalDispatch2 traits.

#[cfg(feature = "smithay_android")]
smithay::delegate_dispatch2!(AndroidSeatRuntime);
