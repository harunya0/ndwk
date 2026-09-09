#include "lang_detector.h"
#include "sherpa-onnx/c-api/c-api.h"
#include "config.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#define LANG_CODE(c1, c2) ((uint16_t)(c1) | ((uint16_t)(c2) << 8))

struct lang_detector_t {
    const SherpaOnnxSpokenLanguageIdentification *slid;
};

static lang_detector_t g_lang_detector;

ndwk_lang_t lang_from_string(const char *str) {
    if (!str || str[0] == '\0' || str[1] == '\0') return NDWK_LANG_JA;
    uint16_t code = (uint8_t)str[0] | ((uint16_t)(uint8_t)str[1] << 8);
    switch (code) {
        case LANG_CODE('j', 'a'): return NDWK_LANG_JA;
        case LANG_CODE('z', 'h'): return NDWK_LANG_ZH;
        case LANG_CODE('e', 'n'): return NDWK_LANG_EN;
        case LANG_CODE('k', 'o'): return NDWK_LANG_KO;
        default:                  return NDWK_LANG_JA;
    }
}

const char *lang_to_string(ndwk_lang_t lang) {
    switch (lang) {
        case NDWK_LANG_JA: return "ja";
        case NDWK_LANG_ZH: return "zh";
        case NDWK_LANG_EN: return "en";
        case NDWK_LANG_KO: return "ko";
        default: return "ja"; // デフォルトは日本語
    }
}

lang_detector_t *lang_detector_create(const char *models_dir) {
    lang_detector_t *d = &g_lang_detector;
    memset(d, 0, sizeof(*d));

    static char enc[512], dec[512];
    snprintf(enc, sizeof(enc), "%s/sherpa-onnx-whisper-tiny/tiny-encoder.int8.onnx", models_dir);
    snprintf(dec, sizeof(dec), "%s/sherpa-onnx-whisper-tiny/tiny-decoder.int8.onnx", models_dir);

    SherpaOnnxSpokenLanguageIdentificationConfig config;
    memset(&config, 0, sizeof(config));
    config.whisper.encoder = enc;
    config.whisper.decoder = dec;
    config.num_threads = 2;
    config.provider = "cpu";

    d->slid = SherpaOnnxCreateSpokenLanguageIdentification(&config);
    if (!d->slid) {
        fprintf(stderr, "[lang_detector] Failed to create SLID\n");
        return NULL;
    }
    return d;
}

void lang_detector_destroy(lang_detector_t *detector) {
    if (detector) {
        if (detector->slid) {
            SherpaOnnxDestroySpokenLanguageIdentification(detector->slid);
        }
    }
}

ndwk_lang_t lang_detector_detect(lang_detector_t *detector, const float *samples, size_t num_samples) {
    if (!detector || !detector->slid || !samples || num_samples == 0) return NDWK_LANG_JA;

    SherpaOnnxOfflineStream *stream =
            SherpaOnnxSpokenLanguageIdentificationCreateOfflineStream(detector->slid);
    
    size_t feed_samples = num_samples;
    if (feed_samples > NDWK_LID_MAX_SAMPLES) {
        feed_samples = NDWK_LID_MAX_SAMPLES; // 最大4秒分の音声を使用
    }

   SherpaOnnxAcceptWaveformOffline(stream, 16000, samples, (int32_t)feed_samples); 

    const SherpaOnnxSpokenLanguageIdentificationResult *res =
            SherpaOnnxSpokenLanguageIdentificationCompute(detector->slid, stream);

    ndwk_lang_t lang = NDWK_LANG_JA; // デフォルトは日本語
    if (res && res->lang) {
        lang = lang_from_string(res->lang);
    }

    SherpaOnnxDestroySpokenLanguageIdentificationResult(res);
    SherpaOnnxDestroyOfflineStream(stream);

    return lang;
}
