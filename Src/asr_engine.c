#include "asr_engine.h"
#include "model_config.h"
#include "sherpa-onnx/c-api/c-api.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

struct asr_engine_t {
    const SherpaOnnxOfflineRecognizer *recognizer;
    char last_text[1024];
};

static asr_engine_t g_asr_engine;

asr_engine_t *asr_engine_create(const char *models_dir, ndwk_lang_t lang) {
    asr_engine_t *engine = &g_asr_engine;
    memset(engine, 0, sizeof(*engine));

    SherpaOnnxOfflineRecognizerConfig config = model_config_create_asr(models_dir, lang);
    engine->recognizer = SherpaOnnxCreateOfflineRecognizer(&config);

    if (!engine->recognizer) {
        fprintf(stderr, "[asr_engine] Failed to create recognizer\n");
        return NULL;
    }
    return engine;
}

void asr_engine_destroy(asr_engine_t *engine) {
    if (engine) {
        if (engine->recognizer) {
            SherpaOnnxDestroyOfflineRecognizer(engine->recognizer);
        }
    }
}

const char *asr_engine_transcribe(const asr_engine_t *engine, const float *samples, size_t num_samples) {
    if (!engine || !engine->recognizer || !samples || num_samples == 0) {
        return "";
    }
    
    const SherpaOnnxOfflineStream *stream = SherpaOnnxCreateOfflineStream(engine->recognizer);
    if (!stream) {
        return "";
    }
    SherpaOnnxAcceptWaveformOffline(stream, 16000, samples, (int32_t)num_samples);
    SherpaOnnxDecodeOfflineStream(engine->recognizer, stream);

    const SherpaOnnxOfflineRecognizerResult *result = SherpaOnnxGetOfflineStreamResult(stream);

    asr_engine_t *mutable_engine = (asr_engine_t *)engine; // キャストして書き込み可能にする
    if (result && result->text) {
        size_t len = strlen(result->text);
        if (len >= sizeof(mutable_engine->last_text)) {
            len = sizeof(mutable_engine->last_text) - 1; // バッファあふれ防止
        }
        memcpy(mutable_engine->last_text, result->text, len);
        mutable_engine->last_text[len] = '\0';
    } else {
        mutable_engine->last_text[0] = '\0';
    }

    SherpaOnnxDestroyOfflineRecognizerResult(result);
    SherpaOnnxDestroyOfflineStream(stream);

    return mutable_engine->last_text;
}
