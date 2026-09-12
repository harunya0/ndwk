use std::ffi::{CStr, CString};
use std::os::raw::{c_char, c_void};

#[repr(C)]
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum NdwkLang {
    Ja = 0,
    Zh = 1,
    Ko = 2,
    En = 3,
    Omni = 4,
    Count = 5,
}

type NdwkOnPartialCb = Option<unsafe extern "C" fn(text: *const c_char, user_data: *mut c_void)>;
type NdwkOnFinalCb = Option<unsafe extern "C" fn(lang: NdwkLang, text: *const c_char, user_data: *mut c_void)>;

#[repr(C)]
struct NdwkConfigRaw {
    models_dir: *const c_char,
    default_lang: NdwkLang,
    auto_detect: bool,
    enable_punct: bool,
    vad_threshold: f32,
    vad_min_silence_sec: f32,
    vad_min_speech_sec: f32,
    vad_max_speech_sec: f32,
    num_threads: i32,
    partial_interval_sec: f32,
    partial_window_sec: f32,
    preroll_sec: f32,
    on_partial: NdwkOnPartialCb,
    on_final: NdwkOnFinalCb,
    user_data: *mut c_void,
}

// 最新 Rust では extern "C" に unsafe が必要
unsafe extern "C" {
    fn ndwk_default_config() -> NdwkConfigRaw;
    fn ndwk_create(config: *const NdwkConfigRaw) -> *mut c_void;
    fn ndwk_destroy(engine: *mut c_void);
    fn ndwk_feed_audio(engine: *mut c_void, samples: *const f32, num_samples: usize);
    fn ndwk_flush(engine: *mut c_void);
}

struct Callbacks<P, F> {
    on_partial: P,
    on_final: F,
}

pub struct NdwkEngine<P, F>
where
    P: FnMut(&str),
    F: FnMut(NdwkLang, &str),
{
    handle: *mut c_void,
    _callbacks: Box<Callbacks<P, F>>,
}

impl<P, F> NdwkEngine<P, F>
where
    P: FnMut(&str),
    F: FnMut(NdwkLang, &str),
{
    pub fn new(
        models_dir: &str,
        lang: NdwkLang,
        enable_punct: bool,
        on_partial: P,
        on_final: F,
    ) -> Result<Self, &'static str> {
        let mut callbacks = Box::new(Callbacks {
            on_partial,
            on_final,
        });

        let mut config = unsafe { ndwk_default_config() };
        let c_models_dir = CString::new(models_dir).map_err(|_| "Failed to convert models_dir to CString")?;

        config.models_dir = c_models_dir.as_ptr();
        config.default_lang = lang;
        config.enable_punct = enable_punct;
        config.on_partial = Some(Self::trampoline_partial);
        config.on_final = Some(Self::trampoline_final);
        config.user_data = &mut *callbacks as *mut _ as *mut c_void;

        let handle = unsafe { ndwk_create(&config) };
        if handle.is_null() {
            return Err("Failed to create NdwkEngine");
        }

        Ok(Self {
            handle,
            _callbacks: callbacks,
        })
    }

    unsafe extern "C" fn trampoline_partial(text: *const c_char, user_data: *mut c_void) {
        if text.is_null() || user_data.is_null() {
            return;
        }
        unsafe {
            let c_str = CStr::from_ptr(text);
            if let Ok(str_slice) = c_str.to_str() {
                let callbacks = &mut *(user_data as *mut Callbacks<P, F>);
                (callbacks.on_partial)(str_slice);
            }
        }
    }

    unsafe extern "C" fn trampoline_final(lang: NdwkLang, text: *const c_char, user_data: *mut c_void) {
        if text.is_null() || user_data.is_null() {
            return;
        }
        unsafe {
            let c_str = CStr::from_ptr(text);
            if let Ok(str_slice) = c_str.to_str() {
                let callbacks = &mut *(user_data as *mut Callbacks<P, F>);
                (callbacks.on_final)(lang, str_slice);
            }
        }
    }

    /// 音声データ（スライス）をゼロコピーで投入
    pub fn feed_audio(&mut self, samples: &[f32]) {
        if !samples.is_empty() {
            unsafe {
                ndwk_feed_audio(self.handle, samples.as_ptr(), samples.len());
            }
        }
    }

    pub fn flush(&mut self) {
        unsafe {
            ndwk_flush(self.handle);
        }
    }
}

impl<P, F> Drop for NdwkEngine<P, F>
where
    P: FnMut(&str),
    F: FnMut(NdwkLang, &str),
{
    fn drop(&mut self) {
        if !self.handle.is_null() {
            unsafe {
                ndwk_destroy(self.handle);
            }
            self.handle = std::ptr::null_mut();
        }
    }
}
