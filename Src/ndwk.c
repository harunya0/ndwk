#include <malloc.h>
/**
 * @file ndwk.c
 * @brief ndwk 音声認識エンジン本体
 *
 * 責務：
 * - VAD、AudioHistory、ASR、PunctEngine の各モジュールを統合
 * - 外部から注入されたPCM音声サンプルの自律的ストリーミング処理
 * - ゼロ動的確保かつI/O非依存のイベント駆動型設計
 */

#include "ndwk.h"
#include "config.h"
#include "asr_engine.h"
#include "vad_detector.h"
#include "audio_history.h"
#include "lang_detector.h"
#include "punct_engine.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ===========================================================================
 * 構造体定義
 * ========================================================================= */

struct ndwk_t {
    ndwk_config_t config;

    asr_engine_t *asr;
    vad_detector_t *vad;
    audio_history_t *history;
    lang_detector_t *lid;
    punct_engine_t *punct;

    ndwk_lang_t current_lang;
    char last_patial[1024];

    size_t samples_since_partial;
    size_t speech_samples;
};

/**
 * @brief 静的インスタンス領域 (ゼロ malloc)
 */
static ndwk_t g_ndwk_instance;

static void ndwk_process_final_segment(ndwk_t *p) {
    vad_segment_t seg;
    size_t preroll_samples = (size_t)(p->config.preroll_sec * NDWK_SAMPLE_RATE);

    while (vad_detector_pop_segment(p->vad, &seg)) {
        size_t full_samples = 0;
        float *full_audio = audio_history_with_preroll(
            p->history,
            seg.start_sample,
            seg.samples,
            seg.num_samples,
            preroll_samples,
            &full_samples
        );

        if (p->config.auto_detect && p->lid) {
            ndwk_lang_t detected_lang = lang_detector_detect(p->lid, full_audio, full_samples);
            if (detected_lang != p->current_lang) {
                asr_engine_destroy(p->asr);
                p->asr = asr_engine_create(&p->config, p->current_lang);
                p->current_lang = detected_lang;
            }
        }

        if (p->asr) {
            const char *final_text = asr_engine_transcribe(p->asr, full_audio, full_samples);
            const char *display_text = final_text;

            if (p->config.enable_punct && p->current_lang == NDWK_LANG_JA && p->punct && final_text && final_text[0] != '\0') {
                display_text = punct_engine_restore(p->punct, final_text);
            }

            if (p->config.on_final && display_text && display_text[0] != '\0') {
                p->config.on_final(p->current_lang, display_text, p->config.user_data);
            }
            p->last_patial[0] = '\0';
        }
    }
}

ndwk_config_t ndwk_default_config(void) {
    ndwk_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.models_dir = "models";
    cfg.default_lang = NDWK_LANG_JA;
    cfg.auto_detect = false;
    cfg.enable_punct = true;
    cfg.vad_threshold = NDWK_VAD_THRESHOLD;
    cfg.vad_min_silence_sec = NDWK_VAD_MIN_SILENCE_SEC;
    cfg.vad_min_speech_sec = NDWK_VAD_MIN_SPEECH_SEC;
    cfg.vad_max_speech_sec = NDWK_VAD_MAX_SPEECH_SEC;
    cfg.num_threads = NDWK_NUM_THREADS;
    cfg.partial_interval_sec = NDWK_PARTIAL_INTERVAL_SEC;
    cfg.partial_window_sec = NDWK_PARTIAL_WINDOW_SEC;
    cfg.preroll_sec = NDWK_PREROLL_SEC;
    cfg.on_partial = NULL;
    cfg.on_final = NULL;
    cfg.user_data = NULL;
    return cfg;
}

ndwk_t *ndwk_create(const ndwk_config_t *config) {
    if (!config) return NULL;
    ndwk_t *p = &g_ndwk_instance;
    memset(p, 0, sizeof(ndwk_t));
    p->config = *config;
    p->current_lang = config->default_lang;
    // サブモジュール初期化
    p->vad = vad_detector_create(config);
    p->history = audio_history_create(NDWK_SAMPLE_RATE, NDWK_HISTORY_KEEP_SEC);
    if (config->enable_punct) {
        p->punct = punct_engine_create();
    }
    if (config->auto_detect) {
        p->lid = lang_detector_create(config->models_dir);
    }
    p->asr = asr_engine_create(config, config->default_lang);
    if (!p->asr || !p->vad || !p->history || (config->auto_detect && !p->lid)) {
        ndwk_destroy(p);
        return NULL;
    }
#ifdef __linux__
    malloc_trim(0);
#endif
    return p;
}

void ndwk_destroy(ndwk_t *p) {
    if (!p) return;
    if (p->asr) asr_engine_destroy(p->asr);
    if (p->vad) vad_detector_destroy(p->vad);
    if (p->lid) lang_detector_destroy(p->lid);
}

void ndwk_feed_audio(ndwk_t *p, const float *samples, size_t num_samples) {
    if (!p || !samples || num_samples == 0) return;

    const size_t chunk_size = NDWK_VAD_WINDOW_SIZE;
    size_t offset = 0;

    size_t interval_samples = (size_t)(p->config.partial_interval_sec * NDWK_SAMPLE_RATE);
    size_t min_speech_samples = (size_t)(p->config.vad_min_speech_sec * NDWK_SAMPLE_RATE);
    size_t max_speech_samples = (size_t)(p->config.partial_window_sec * NDWK_SAMPLE_RATE);
    size_t preroll_samples = (size_t)(p->config.preroll_sec * NDWK_SAMPLE_RATE);

    while (offset < num_samples) {
        size_t n = chunk_size;
        if (offset + n > num_samples) {
            n = num_samples - offset;
        }

        audio_history_push(p->history, samples + offset, n);
        vad_detector_accept(p->vad, samples + offset, n);
        offset += n;
        p->samples_since_partial += n;

        bool is_speech = vad_detector_is_speech(p->vad);
        if (is_speech) {
            p->speech_samples += n;
        } else {
            p->speech_samples = 0;
        }

        if (p->samples_since_partial >= interval_samples && is_speech && p->speech_samples >= min_speech_samples) {
            p->samples_since_partial = 0;
            size_t target_samples = p->speech_samples + preroll_samples;
            if (target_samples > max_speech_samples) {
                target_samples = max_speech_samples;
            }

            size_t recent_n = 0;
            const float *recent_audio = audio_history_get_recent(p->history, target_samples, &recent_n);
            if (recent_audio && p->asr) {
                const char *partial_text = asr_engine_transcribe(p->asr, recent_audio, recent_n);
                if (partial_text && partial_text[0] != '\0') {
                    if (strcmp(partial_text, p->last_patial) != 0) {
                        strncpy(p->last_patial, partial_text, sizeof(p->last_patial) - 1);
                        p->last_patial[sizeof(p->last_patial) - 1] = '\0';
                        if (p->config.on_partial) {
                            p->config.on_partial(partial_text, p->config.user_data);
                        }
                    }
                }
            }
        }
        ndwk_process_final_segment(p);
    }
}

void ndwk_flush(ndwk_t *p) {
    if (!p) return;
    vad_detector_flush(p->vad);
    ndwk_process_final_segment(p);
}
