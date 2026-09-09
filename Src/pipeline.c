#include "pipeline.h"
#include "config.h"
#include "model_config.h"
#include "vad_detector.h"
#include "asr_engine.h"
#include "audio_history.h"
#include "lang_detector.h"
#include "mic_reader.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <signal.h>

struct pipeline_t {
    const char *models_dir;
    bool auto_detect;
    ndwk_lang_t current_lang;
    asr_engine_t *asr;
    vad_detector_t *vad;
    audio_history_t *history;
    lang_detector_t *lid;
    char last_partial[512];
};

static pipeline_t g_pipeline;

pipeline_t *pipeline_create(const char *models_dir, ndwk_lang_t default_lang, bool auto_detect) {
    pipeline_t *p = &g_pipeline;
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
    if (p->lid) lang_detector_destroy(p->lid);
}

static void pipeline_process_final_segment(pipeline_t *p) {
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
            p->last_partial[0] = '\0';
        }
    }
}

void pipeline_run_wav(pipeline_t *p, const wav_data_t *wav) {
    if (!p || !wav || !wav->samples) return;

    printf("=== ndwk Real-time Streaming Speech Recognition ===\n");

    size_t offset = 0;
    const size_t chunk_size = NDWK_VAD_WINDOW_SIZE; // 512 samples (32ms)
    size_t samples_since_partial = 0;

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
        if (samples_since_partial >= NDWK_PARTIAL_INTERVAL_SAMPLES && vad_detector_is_speech(p->vad)) {
            samples_since_partial = 0;

            size_t recent_n = 0;
            const float *recent_audio = audio_history_get_recent(p->history, NDWK_PARTIAL_WINDOW_SAMPLES, &recent_n);
            if (recent_audio && p->asr) {
                const char *partial_text = asr_engine_transcribe(p->asr, recent_audio, recent_n);
                if (partial_text && partial_text[0] != '\0') {
                    if (strcmp(partial_text, p->last_partial) != 0) {
                        printf("\033[2K\r~ %s", partial_text);
                        fflush(stdout);
                        strncpy(p->last_partial, partial_text, sizeof(p->last_partial) - 1);
                        p->last_partial[sizeof(p->last_partial) - 1] = '\0';
                    }
                }
            }
        }
        // 2. 発話終了時の確定表示
        pipeline_process_final_segment(p);

        struct timespec now;
        clock_gettime(CLOCK_MONOTONIC, &now);
        int64_t audio_ns = (int64_t)offset * 62500LL;
        int64_t real_ns  = (int64_t)(now.tv_sec - start_time.tv_sec) * 1000000000LL +
                        (int64_t)(now.tv_nsec - start_time.tv_nsec);

        if (audio_ns > real_ns) {
            int64_t diff_ns = audio_ns - real_ns;
            struct timespec req = {
                .tv_sec  = 0,
                .tv_nsec = diff_ns
            };
            nanosleep(&req, NULL);
        }
    }


    vad_detector_flush(p->vad);
    pipeline_process_final_segment(p);
}

static volatile bool g_mic_running = true;
static void sigint_handler(int signum) {
    (void)signum;
    g_mic_running = false;
}

void pipeline_run_mic(pipeline_t *p) {
    if (!p) return;

    mic_reader_t *mic = mic_reader_create(NDWK_SAMPLE_RATE);
    if (!mic || !mic_reader_start(mic)) {
        fprintf(stderr, "Failed to initialize microphone reader.\n");
        if (mic) mic_reader_destroy(mic);
        return;
    }

    g_mic_running = true;
    signal(SIGINT, sigint_handler);

    printf("=== ndwk Real-time Streaming Speech Recognition (Mic) ===\n");
    printf("Listening... Press Ctrl+C to stop.\n");

    const size_t chunk_size = NDWK_VAD_WINDOW_SIZE;

    float chunk[NDWK_VAD_WINDOW_SIZE];
    size_t samples_since_partial = 0;

    while (g_mic_running) {
        size_t n = mic_reader_read(mic, chunk, chunk_size);
        if (n == 0) {
            usleep(5000); // 5ms待って次のサンプルを待つ
            continue;
        }

        audio_history_push(p->history, chunk, n);
        vad_detector_accept(p->vad, chunk, n);
        samples_since_partial += n;
        if(samples_since_partial >= NDWK_PARTIAL_INTERVAL_SAMPLES && vad_detector_is_speech(p->vad)) {
            samples_since_partial = 0;

            size_t recent_n = 0;
            const float *recent_audio = audio_history_get_recent(p->history, NDWK_PARTIAL_WINDOW_SAMPLES, &recent_n);

            if (recent_audio && p->asr) {
                const char *partial_text = asr_engine_transcribe(p->asr, recent_audio, recent_n);
                if (partial_text && partial_text[0] != '\0') {
                    if (strcmp(partial_text, p->last_partial) != 0) {
                        printf("\033[2K\r~ %s", partial_text);
                        fflush(stdout);
                        strncpy(p->last_partial, partial_text, sizeof(p->last_partial) - 1);
                        p->last_partial[sizeof(p->last_partial) - 1] = '\0';
                    }
                }
            }
        }

        pipeline_process_final_segment(p);
    }

    printf("\nStopping microphone reader...\n");

    // 終了時に未確定の音声が残っていれば flush して文字起こし
    vad_detector_flush(p->vad);
    pipeline_process_final_segment(p);

    mic_reader_stop(mic);
    mic_reader_destroy(mic);
}
