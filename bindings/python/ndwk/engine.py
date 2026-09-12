import os
from pathlib import Path
import ctypes.util

def _find_library():
    if "NDWK_LIB_PATH" in os.environ:
        p = Path(os.environ["NDWK_LIB_PATH"])
        if p.exists():
            return str(p)

    candidates = [
        Path(__file__).resolve().parent.parent.parent.parent /"build" / "libndwk.so",
        Path("build/libndwk.so"),
        Path("../../build/libndwk.so"),
        Path("libndwk.so"),
    ]
    for c in candidates:
        if c.exists():
            return str(c.resolve())

    found = ctypes.util.find_library("ndwk")
    if found:
        return found

    return "libndwk.so"

import ctypes
from ctypes import (
    c_char_p, c_int, c_bool, c_float, c_int32, c_size_t, c_void_p,
    POINTER, CFUNCTYPE, CDLL, byref
)
from enum import IntEnum
from typing import Callable, Optional, Union
import array

class NdwkLang(IntEnum):
    JA = 0
    ZH = 1
    KO = 2
    EN = 3
    OMNI = 4

PARTIAL_CB = CFUNCTYPE(None, c_char_p, c_void_p)
FINAL_CB = CFUNCTYPE(None, c_int, c_char_p, c_void_p)

class NdwkConfig(ctypes.Structure):
    _fields_ = [
        ("models_dir", c_char_p),
        ("default_lang", c_int),
        ("auto_detect", c_bool),
        ("enable_punct", c_bool),
        ("vad_threshold", c_float),
        ("vad_min_silence_sec", c_float),
        ("vad_min_speech_sec", c_float),
        ("vad_max_speech_sec", c_float),
        ("num_threads", c_int32),
        ("partial_interval_sec", c_float),
        ("partial_window_sec", c_float),
        ("preroll_sec", c_float),
        ("on_partial", PARTIAL_CB),
        ("on_final", FINAL_CB),
        ("user_data", c_void_p),
    ]

_lib_path = _find_library()
_lib = CDLL(_lib_path)
_lib.ndwk_default_config.restype = NdwkConfig
_lib.ndwk_create.argtypes = [POINTER(NdwkConfig)]
_lib.ndwk_create.restype = c_void_p
_lib.ndwk_feed_audio.argtypes = [c_void_p, POINTER(c_float), c_size_t]
_lib.ndwk_feed_audio.restype = None
_lib.ndwk_flush.argtypes = [c_void_p]
_lib.ndwk_flush.restype = None
_lib.ndwk_destroy.argtypes = [c_void_p]
_lib.ndwk_destroy.restype = None

class NdwkEngine:
    def __init__(
        self,
        models_dir: str = "models",
        lang: NdwkLang = NdwkLang.JA,
        enable_punct: bool = True,
        on_partial: Optional[Callable[[str], None]] = None,
        on_final: Optional[Callable[[NdwkLang, str], None]] = None,
    ):
        self.on_partial = on_partial
        self.on_final = on_final
        cfg = _lib.ndwk_default_config()
        self._models_dir_bytes = models_dir.encode("utf-8")
        cfg.models_dir = self._models_dir_bytes
        cfg.default_lang = int(lang)
        cfg.enable_punct = enable_punct
        self._c_partial = PARTIAL_CB(self._wrap_partial)
        self._c_final = FINAL_CB(self._wrap_final)
        cfg.on_partial = self._c_partial
        cfg.on_final = self._c_final
        self._handle = _lib.ndwk_create(byref(cfg))
        if not self._handle:
            raise RuntimeError("Failed to create ndwk engine")
    def _wrap_partial(self, text_ptr: bytes, user_data: int):
        if self.on_partial and text_ptr:
            self.on_partial(text_ptr.decode("utf-8"))
    def _wrap_final(self, lang_val: int, text_ptr: bytes, user_data: int):
        if self.on_final and text_ptr:
            self.on_final(NdwkLang(lang_val), text_ptr.decode("utf-8"))
    def feed_audio(self, samples: Union[array.array, list, tuple]):
        """16kHz float32 音声データを投入（ゼロコピー対応）"""
        if not self._handle or len(samples) == 0:
            return
        # array.array('f') の場合はゼロコピー
        if isinstance(samples, array.array) and samples.typecode == "f":
            ptr = ctypes.cast(samples.buffer_info()[0], POINTER(c_float))
            _lib.ndwk_feed_audio(self._handle, ptr, len(samples))
        # NumPy 配列の場合
        elif hasattr(samples, "__array_interface__"):
            ptr = samples.ctypes.data_as(POINTER(c_float)) # type: ignore[union-attr]
            _lib.ndwk_feed_audio(self._handle, ptr, len(samples))
        # 通常の list / tuple の場合
        else:
            c_arr = (c_float * len(samples))(*samples)
            _lib.ndwk_feed_audio(self._handle, c_arr, len(samples))
    def flush(self):
        if self._handle:
            _lib.ndwk_flush(self._handle)
    def close(self):
        if self._handle:
            _lib.ndwk_destroy(self._handle)
            self._handle = None
    def __enter__(self):
        return self
    def __exit__(self, exc_type, exc_val, exc_tb):
        self.close()
    def __del__(self):
        self.close()
