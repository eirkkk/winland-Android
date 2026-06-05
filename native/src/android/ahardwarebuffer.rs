use std::ffi::CStr;
use std::os::unix::io::RawFd;

/// Matches android's `AHardwareBuffer_Desc` layout
#[repr(C)]
pub struct AHardwareBufferDesc {
    pub width: u32,
    pub height: u32,
    pub layers: u32,
    pub format: u32,
    pub usage: u64,
    pub stride: u32,
    pub rfu0: u32,
    pub rfu1: u64,
}

/// Matches android's `native_handle_t` with exactly 1 fd / 0 ints.
/// Layout: version(4) + numFds(4) + numInts(4) + data[1](4) = 16 bytes
#[repr(C)]
pub struct NativeHandle1Fd {
    pub version: i32,
    pub num_fds: i32,
    pub num_ints: i32,
    pub fd: i32,
}

pub const AHARDWAREBUFFER_FORMAT_R8G8B8A8_UNORM: u32 = 1;
pub const AHARDWAREBUFFER_FORMAT_R8G8B8X8_UNORM: u32 = 2;
pub const AHARDWAREBUFFER_FORMAT_R5G6B5_UNORM: u32 = 4;
pub const AHARDWAREBUFFER_FORMAT_R8G8B8_UNORM: u32 = 3;

pub const AHARDWAREBUFFER_USAGE_GPU_SAMPLED_IMAGE: u64 = 1 << 8;
pub const AHARDWAREBUFFER_USAGE_GPU_COLOR_OUTPUT: u64 = 1 << 9;
pub const AHARDWAREBUFFER_USAGE_CPU_READ_RARELY: u64 = 2;
pub const AHARDWAREBUFFER_USAGE_CPU_WRITE_RARELY: u64 = 2 << 4;

pub const AHARDWAREBUFFER_CREATE_FROM_HANDLE_METHOD_CLONE: i32 = 1;

#[link(name = "android")]
extern "C" {
    pub fn AHardwareBuffer_release(buffer: *mut std::ffi::c_void);

    #[allow(dead_code)]
    pub fn AHardwareBuffer_describe(
        buffer: *const std::ffi::c_void,
        out_desc: *mut AHardwareBufferDesc,
    );

    pub fn native_handle_delete(handle: *mut std::ffi::c_void) -> i32;
}

type CreateFromHandleFn = unsafe extern "C" fn(
    desc: *const AHardwareBufferDesc,
    handle: *const std::ffi::c_void,
    method: i32,
    out_buffer: *mut *mut std::ffi::c_void,
) -> i32;

/// Load `AHardwareBuffer_createFromHandle` via dlsym at runtime.
/// Falls back to `None` if the symbol is not available (API < 26 or OEM ROM).
pub fn get_create_from_handle() -> Option<CreateFromHandleFn> {
    static FN: std::sync::OnceLock<Option<CreateFromHandleFn>> = std::sync::OnceLock::new();
    *FN.get_or_init(|| {
        let name = match CStr::from_bytes_with_nul(b"AHardwareBuffer_createFromHandle\0") {
            Ok(n) => n,
            Err(_) => return None,
        };
        let ptr = unsafe { libc::dlsym(libc::RTLD_DEFAULT, name.as_ptr()) };
        if ptr.is_null() {
            log::warn!("AHardwareBuffer_createFromHandle not found (dlsym)");
            None
        } else {
            Some(unsafe { std::mem::transmute::<*mut std::ffi::c_void, CreateFromHandleFn>(ptr) })
        }
    })
}

/// Build a stack-allocated `native_handle_t` for a single-fd dmabuf plane.
pub fn make_handle_1fd(fd: RawFd) -> NativeHandle1Fd {
    NativeHandle1Fd {
        version: std::mem::size_of::<NativeHandle1Fd>() as i32,
        num_fds: 1,
        num_ints: 0,
        fd,
    }
}

pub fn fourcc_to_ahb_format(code: smithay::backend::allocator::Fourcc) -> u32 {
    use smithay::backend::allocator::Fourcc;
    match code {
        Fourcc::Abgr8888 => AHARDWAREBUFFER_FORMAT_R8G8B8A8_UNORM,
        Fourcc::Xbgr8888 => AHARDWAREBUFFER_FORMAT_R8G8B8X8_UNORM,
        Fourcc::Rgb565 => AHARDWAREBUFFER_FORMAT_R5G6B5_UNORM,
        Fourcc::Bgr888 => AHARDWAREBUFFER_FORMAT_R8G8B8_UNORM,
        _ => {
            log::warn!("unmapped fourcc {code:?}, defaulting RGBA8888");
            AHARDWAREBUFFER_FORMAT_R8G8B8A8_UNORM
        }
    }
}
