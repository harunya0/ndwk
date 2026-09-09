#include "pipeline.h"
#include "config.h"
#include "model_config.h"
#include "vad_detector.h"
#include "asr_engine.h"
#include "audio_history.h"
#include "lang_detector.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

struct pipeline_t {
    const char *models_dir;
    bool auto_detect;
    ndwk_lang_t current_lang;
    asr_engine_t *asr;
    vad_detector_t *vad;
    audio_history_t *history;
    lang_detector_t *lid;
};

pipeline_t *pipeline_create(const char *models_dir, ndwk_lang_t default_lang, bool auto_detect) {
    pipeline_t *p = (pipeline_t *)malloc(sizeof(pipeline_t));
    if (!p) return NULL;
    memset(p, 0, sizeof(pipeline_t));

    p->models_dir = models_dir;
    p->auto_detect = auto_detect;
    p->current_lang = default_lang;
    
    p->vad = vad_detector_create(models_dir);
    p->history = audio_history_create(16000, 10.0f);

    if (auto_detect) {
        p->lid = lang_detector_create(models_dir);
        // 初期状態として日本語モデルを用意しておく (速報表示用)
        p->asr = asr_engine_create(models_dir, default_lang);
    } else {
        p->lid = NULL;
        p->asr = asr_engine_create(models_dir, default_lang);
    }

    if (!p->asr || !p->vad || !p->history || (auto_detect && !p->lid)) {
        pipeline_destroy(p);
        return NULL;
    }

    return p;
}

void pipeline_destroy(pipeline_t *p) {
    if (!p) return;
    if (p->asr) asr_engine_destroy(p->asr);
    if (p->vad) vad_detector_destroy(p->vad);
    if (p->history) audio_history_destroy(p->history);
    if (p->lid) lang_detector_destroy(p->lid);
    free(p);
}

void pipeline_run_wav(pipeline_t *p, const wav_data_t *wav) {
    if (!p || !wav || !wav->samples) return;

    printf("=== ndwk Real-time Streaming Speech Recognition ===\n");

    size_t offset = 0;
    const size_t chunk_size = NDWK_VAD_WINDOW_SIZE; // 512 samples (32ms)
    const size_t partial_interval = (size_t)(NDWK_SAMPLE_RATE * NDWK_PARTIAL_INTERVAL_SEC);
    const size_t max_partial_samples = (size_t)(NDWK_SAMPLE_RATE * NDWK_PARTIAL_WINDOW_SEC);
    size_t samples_since_partial = 0;
    int last_partial_len = 0;

    struct timespec start_time;
    clock_gettime(CLOCK_MONOTONIC, &start_time);

    while (offset < wav->num_samples) {
        size_t n = chunk_size;
        if (offset + n > wav->num_samples) {
            n = wav->num_samples - offset;
        }

        audio_history_push(p->history, wav->samples + offset, n);
        vad_detector_accept(p->vad, wav->samples + offset, n);

        offset += n;
        samples_since_partial += n;

        // 1. 発話中の速報表示 (直近 max_partial_samples に絞って高速化)
        if (vad_detector_is_speech(p->vad) && samples_since_partial >= partial_interval) {
            samples_since_partial = 0;

            size_t recent_n = 0;
            const float *recent_audio = audio_history_get_recent(p->history, max_partial_samples, &recent_n);
            if (recent_audio && p->asr) {
                const char *partial_text = asr_engine_transcribe(p->asr, recent_audio, recent_n);
                if (partial_text && partial_text[0] != '\0') {
                    printf("\r~ %s", partial_text);
                    int pad = last_partial_len - (int)strlen(partial_text);
                    for (int i = 0; i < pad; i++) putchar(' ');
                    fflush(stdout);
                    last_partial_len = (int)strlen(partial_text);
                }
            }
        }
        // 2. 発話終了時の確定表示
        vad_segment_t seg;
        while (vad_detector_pop_segment(p->vad, &seg)) {
            size_t full_samples = 0;
            float *full_audio = audio_history_with_preroll(
                    p->history, seg.start_sample, seg.samples, seg.num_samples, &full_samples);

            if (p->auto_detect && p->lid) {
                ndwk_lang_t detected = lang_detector_detect(p->lid, full_audio, full_samples);
                if (detected != p->current_lang) {
                    asr_engine_destroy(p->asr);
                    p->asr = asr_engine_create(p->models_dir, detected);
                    p->current_lang = detected;
                }
            }

            if (p->asr) {
                const char *final_text = asr_engine_transcribe(p->asr, full_audio, full_samples);
                printf("\r[確定 (%s)]: %s\n", lang_to_string(p->current_lang), final_text);
                fflush(stdout);
                last_partial_len = 0;
            }

            free(full_audio);
        }

        struct timespec now;
        clock_gettime(CLOCK_MONOTONIC, &now);
        double elapsed_real = (now.tv_sec - start_time.tv_sec) + (now.tv_nsec - start_time.tv_nsec) / 1e9;
        double audio_time = (double)offset / NDWK_SAMPLE_RATE;

        if (audio_time > elapsed_real) {
            useconds_t sleep_us = (useconds_t)((audio_time - elapsed_real) * 1e6);
            usleep(sleep_us);
        }
    }

    vad_detector_flush(p->vad);
    vad_segment_t seg;
    while (vad_detector_pop_segment(p->vad, &seg)) {
        size_t full_samples = 0;
        float *full_audio = audio_history_with_preroll(
                p->history, seg.start_sample, seg.samples, seg.num_samples, &full_samples);

        if (p->auto_detect && p->lid) {
            ndwk_lang_t detected = lang_detector_detect(p->lid, full_audio, full_samples);
            if (detected != p->current_lang) {
                asr_engine_destroy(p->asr);
                p->asr = asr_engine_create(p->models_dir, detected);
                p->current_lang = detected;
            }
        }

        if (p->asr) {
            const char *final_text = asr_engine_transcribe(p->asr, full_audio, full_samples);
            printf("\r[確定 (%s)]: %s\n", lang_to_string(p->current_lang), final_text);
            fflush(stdout);
            last_partial_len = 0;
        }

        free(full_audio);
    }
}
