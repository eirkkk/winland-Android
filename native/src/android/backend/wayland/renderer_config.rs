use smithay::backend::allocator::{Format, Fourcc, Modifier};

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub(crate) enum RendererBackend {
    Dmabuf,
    Shm,
}

impl RendererBackend {
    pub(crate) fn detect() -> Self {
        if let Ok(val) = std::env::var("WINLAND_RENDERER") {
            match val.to_lowercase().as_str() {
                "dmabuf" => RendererBackend::Dmabuf,
                _ => RendererBackend::Shm,
            }
        } else {
            RendererBackend::auto_detect()
        }
    }

    pub(crate) fn is_dmabuf(&self) -> bool {
        matches!(self, RendererBackend::Dmabuf)
    }

    fn auto_detect() -> Self {
        if cfg!(target_os = "android") {
            if std::path::Path::new("/dev/kgsl-3d0").exists() {
                log::info!("RendererBackend: auto → dmabuf (kgsl detected)");
                return RendererBackend::Dmabuf;
            }
        }
        log::info!("RendererBackend: auto → SHM fallback");
        RendererBackend::Shm
    }
}

pub(crate) unsafe fn query_egl_dmabuf_formats(
    egl_display: *mut std::ffi::c_void,
    get_proc_addr: unsafe extern "C" fn(*const std::ffi::c_char) -> *mut std::ffi::c_void,
) -> Result<Vec<Format>, String> {
    let query_formats_name = std::ffi::CString::new("eglQueryDmaBufFormatsEXT").unwrap();
    let query_modifiers_name = std::ffi::CString::new("eglQueryDmaBufModifiersEXT").unwrap();

    if egl_display.is_null() {
        log::warn!("query_egl_dmabuf_formats: egl_display is null");
        return Ok(vec![]);
    }

    let query_formats_ptr = get_proc_addr(query_formats_name.as_ptr());
    let query_modifiers_ptr = get_proc_addr(query_modifiers_name.as_ptr());

    if query_formats_ptr.is_null() || query_modifiers_ptr.is_null() {
        log::warn!(
            "query_egl_dmabuf_formats: EGL_EXT_image_dma_buf_import_modifiers not available"
        );
        return Ok(vec![]);
    }

    let egl_query_formats = std::mem::transmute::<
        *mut std::ffi::c_void,
        unsafe extern "C" fn(*mut std::ffi::c_void, i32, *mut u32, *mut i32) -> u32,
    >(query_formats_ptr);
    let egl_query_modifiers = std::mem::transmute::<
        *mut std::ffi::c_void,
        unsafe extern "C" fn(*mut std::ffi::c_void, i32, i32, *mut u64, *mut u8, *mut i32) -> u32,
    >(query_modifiers_ptr);

    let mut num: i32 = 0;
    if egl_query_formats(egl_display, 0, std::ptr::null_mut(), &mut num) == 0 || num == 0 {
        return Ok(vec![]);
    }

    let mut fourccs: Vec<u32> = vec![0u32; num as usize];
    if egl_query_formats(egl_display, num, fourccs.as_mut_ptr(), &mut num) == 0 {
        return Err("eglQueryDmaBufFormatsEXT failed".to_string());
    }

    let mut formats = Vec::new();
    for &fourcc in &fourccs {
        let Ok(code) = Fourcc::try_from(fourcc) else {
            continue;
        };
        let mut num_mods: i32 = 0;
        if egl_query_modifiers(
            egl_display,
            fourcc as i32,
            0,
            std::ptr::null_mut(),
            std::ptr::null_mut(),
            &mut num_mods,
        ) == 0
            || num_mods == 0
        {
            formats.push(Format {
                code,
                modifier: Modifier::Linear,
            });
            continue;
        }
        let mut modifiers = vec![0u64; num_mods as usize];
        let mut has_mods = vec![0u8; num_mods as usize];
        if egl_query_modifiers(
            egl_display,
            fourcc as i32,
            num_mods,
            modifiers.as_mut_ptr(),
            has_mods.as_mut_ptr(),
            &mut num_mods,
        ) == 0
        {
            continue;
        }
        for (&mod_val, &has_mod) in modifiers.iter().zip(has_mods.iter()) {
            if has_mod != 0 {
                formats.push(Format {
                    code,
                    modifier: Modifier::from(mod_val),
                });
            }
        }
    }

    Ok(formats)
}
