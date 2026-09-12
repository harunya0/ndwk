package ndwk

/*
#cgo CFLAGS: -I${SRCDIR}/../../Inc
#cgo LDFLAGS: -L${SRCDIR}/../../build -lndwk -Wl,-rpath,${SRCDIR}/../../build
#include "ndwk.h"
#include <stdlib.h>
// Go の関数を呼び出すための C 言語側ブリッジ宣言
#include <stdint.h>

extern void goOnPartialBridge(char *text, uintptr_t userData);
extern void goOnFinalBridge(ndwk_lang_t lang, char *text, uintptr_t userData);

static void c_on_partial(const char *text, void *userData) {
    goOnPartialBridge((char*)text, (uintptr_t)userData);
}

static void c_on_final(ndwk_lang_t lang, const char *text, void *userData) {
    goOnFinalBridge(lang, (char*)text, (uintptr_t)userData);
}

static void setup_ndwk_callbacks(ndwk_config_t *cfg, uintptr_t userData) {
    cfg->on_partial = c_on_partial;
    cfg->on_final = c_on_final;
    cfg->user_data = (void*)userData;
}
*/
import "C"
import (
	"errors"
	"runtime/cgo"
	"unsafe"
)

type Lang int

const (
	LangJa   Lang = C.NDWK_LANG_JA
	LangZh   Lang = C.NDWK_LANG_ZH
	LangKo   Lang = C.NDWK_LANG_KO
	LangEn   Lang = C.NDWK_LANG_EN
	LangOmni Lang = C.NDWK_LANG_OMNI
)

func (l Lang) String() string {
	switch l {
	case LangJa:
		return "ja"
	case LangZh:
		return "zh"
	case LangKo:
		return "ko"
	case LangEn:
		return "en"
	case LangOmni:
		return "omni"
	default:
		return "unknown"
	}
}

type Config struct {
	ModelsDir   string
	Lang        Lang
	EnablePunct bool
	OnPartial   func(text string)
	OnFinal     func(lang Lang, text string)
}

type Engine struct {
	handle    *C.ndwk_t
	cgoHandle cgo.Handle
	onPartial func(text string)
	onFinal   func(lang Lang, text string)
}

//export goOnPartialBridge
func goOnPartialBridge(text *C.char, userData C.uintptr_t) {
	if text == nil || userData == 0 {
		return
	}
	h := cgo.Handle(userData)
	engine := h.Value().(*Engine)
	if engine.onPartial != nil {
		engine.onPartial(C.GoString(text))
	}
}

//export goOnFinalBridge
func goOnFinalBridge(lang C.ndwk_lang_t, text *C.char, userData C.uintptr_t) {
	if text == nil || userData == 0 {
		return
	}
	h := cgo.Handle(userData)
	engine := h.Value().(*Engine)
	if engine.onFinal != nil {
		engine.onFinal(Lang(lang), C.GoString(text))
	}
}

func New(cfg Config) (*Engine, error) {
	cCfg := C.ndwk_default_config()
	cModelsDir := C.CString(cfg.ModelsDir)
	defer C.free(unsafe.Pointer(cModelsDir))
	cCfg.models_dir = cModelsDir
	cCfg.default_lang = C.ndwk_lang_t(cfg.Lang)
	cCfg.enable_punct = C.bool(cfg.EnablePunct)

	engine := &Engine{
		onPartial: cfg.OnPartial,
		onFinal:   cfg.OnFinal,
	}

	engine.cgoHandle = cgo.NewHandle(engine)
	C.setup_ndwk_callbacks(&cCfg, C.uintptr_t(engine.cgoHandle))

	handle := C.ndwk_create(&cCfg)
	if handle == nil {
		engine.cgoHandle.Delete()
		return nil, errors.New("failed to create ndwk engine")
	}
	engine.handle = handle
	return engine, nil
}

func (e *Engine) FeedAudio(samples []float32) {
	if len(samples) == 0 || e.handle == nil {
		return
	}
	C.ndwk_feed_audio(e.handle, (*C.float)(&samples[0]), C.size_t(len(samples)))
}

func (e *Engine) Flush() {
	if e.handle != nil {
		C.ndwk_flush(e.handle)
	}
}

func (e *Engine) Close() {
	if e.handle != nil {
		C.ndwk_destroy(e.handle)
		e.handle = nil
		e.cgoHandle.Delete()
	}
}
