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

/// ndwk 音声認識エンジンの詳細設定
#[derive(Debug, Clone)]
pub struct NdwkConfig {
    pub models_dir: String,
    pub default_lang: NdwkLang,
    pub auto_detect: bool,
    pub enable_punct: bool,
    pub vad_threshold: f32,
    pub vad_min_silence_sec: f32,
    pub vad_min_speech_sec: f32,
    pub vad_max_speech_sec: f32,
    pub num_threads: i32,
    pub partial_interval_sec: f32,
    pub partial_window_sec: f32,
    pub preroll_sec: f32,
}

impl Default for NdwkConfig {
    fn default() -> Self {
        let raw = unsafe { ndwk_default_config() };
        Self {
            models_dir: "models".to_string(),
            default_lang: raw.default_lang,
            auto_detect: raw.auto_detect,
            enable_punct: raw.enable_punct,
            vad_threshold: raw.vad_threshold,
            vad_min_silence_sec: raw.vad_min_silence_sec,
            vad_min_speech_sec: raw.vad_min_speech_sec,
            vad_max_speech_sec: raw.vad_max_speech_sec,
            num_threads: raw.num_threads,
            partial_interval_sec: raw.partial_interval_sec,
            partial_window_sec: raw.partial_window_sec,
            preroll_sec: raw.preroll_sec,
        }
    }
}

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
        let config = NdwkConfig {
            models_dir: models_dir.to_string(),
            default_lang: lang,
            enable_punct,
            ..Default::default()
        };
        Self::with_config(config, on_partial, on_final)
    }

    /// 詳細設定 (NdwkConfig) を指定してエンジンを初期化
    pub fn with_config(
        config: NdwkConfig,
        on_partial: P,
        on_final: F,
    ) -> Result<Self, &'static str> {
        let mut callbacks = Box::new(Callbacks {
            on_partial,
            on_final,
        });

        let mut raw_config = unsafe { ndwk_default_config() };
        let c_models_dir = CString::new(config.models_dir.as_str()).map_err(|_| "Failed to convert models_dir to CString")?;

        raw_config.models_dir = c_models_dir.as_ptr();
        raw_config.default_lang = config.default_lang;
        raw_config.auto_detect = config.auto_detect;
        raw_config.enable_punct = config.enable_punct;
        raw_config.vad_threshold = config.vad_threshold;
        raw_config.vad_min_silence_sec = config.vad_min_silence_sec;
        raw_config.vad_min_speech_sec = config.vad_min_speech_sec;
        raw_config.vad_max_speech_sec = config.vad_max_speech_sec;
        raw_config.num_threads = config.num_threads;
        raw_config.partial_interval_sec = config.partial_interval_sec;
        raw_config.partial_window_sec = config.partial_window_sec;
        raw_config.preroll_sec = config.preroll_sec;
        raw_config.on_partial = Some(Self::trampoline_partial);
        raw_config.on_final = Some(Self::trampoline_final);
        raw_config.user_data = &mut *callbacks as *mut _ as *mut c_void;

        let handle = unsafe { ndwk_create(&raw_config) };
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
