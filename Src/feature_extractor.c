/**
 * @file feature_extractor.c
 * @brief 音響特徴量 (F0, RMS, Energy, ΔF0) 抽出モジュールの実装
 */

#include "feature_extractor.h"
#include <math.h>
#include <string.h>

#define PITCH_MIN_HZ 80.0f
#define PITCH_MAX_HZ 500.0f
#define SAMPLE_RATE 16000.0f
#define LAG_MIN ((size_t)(SAMPLE_RATE / PITCH_MAX_HZ)) // 32
#define LAG_MAX ((size_t)(SAMPLE_RATE / PITCH_MIN_HZ)) // 200

void feature_extractor_init(feature_extractor_t *fe) {
    if (!fe) return;
    fe->prev_f0 = 0.0f;
    fe->silence_frames = 0;
    fe->frame_index = 0;
}

void feature_extractor_process_frame(
    feature_extractor_t *fe,
    const float *samples,
    size_t num_samples,
    bool is_speech,
    float vad_prob,
    int32_t token_id,
    float token_conf,
    ndwk_frame_meta_t *out_meta
) {
    if (!fe || !samples || !out_meta || num_samples == 0) return;

    memset(out_meta, 0, sizeof(*out_meta));
    out_meta->frame_index = fe->frame_index++;
    out_meta->is_speech = is_speech ? 1 : 0;
    out_meta->vad_prob = vad_prob;
    out_meta->token_id = token_id;
    out_meta->token_confidence = token_conf;

    // 1. Energy & RMS
    float sum_sq = 0.0f;
    for (size_t i = 0; i < num_samples; ++i) {
        sum_sq += samples[i] * samples[i];
    }
    out_meta->energy = sum_sq;
    out_meta->rms = sqrtf(sum_sq / (float)num_samples);

    // 2. 無音フレーム数の更新
    if (is_speech) {
        fe->silence_frames = 0;
    } else {
        fe->silence_frames++;
    }
    out_meta->silence_frames = fe->silence_frames;

    // 3. F0 (正規化自己相関法)
    float f0 = 0.0f;
    if (is_speech && out_meta->rms > 0.005f) {
        float r0 = sum_sq;
        float best_r = 0.0f;
        size_t best_lag = 0;

        size_t max_lag = LAG_MAX < num_samples ? LAG_MAX : num_samples;
        for (size_t lag = LAG_MIN; lag < max_lag; ++lag) {
            float r = 0.0f;
            for (size_t i = 0; i < num_samples - lag; ++i) {
                r += samples[i] * samples[i + lag];
            }
            if (r > best_r) {
                best_r = r;
                best_lag = lag;
            }
        }

        // 正規化自己相関が閾値(0.35)以上でピーク検知
        if (r0 > 1e-6f && (best_r / r0) > 0.35f && best_lag > 0) {
            f0 = SAMPLE_RATE / (float)best_lag;
        }
    }
    out_meta->f0 = f0;

    // 4. ΔF0
    if (f0 > 0.0f && fe->prev_f0 > 0.0f) {
        out_meta->delta_f0 = f0 - fe->prev_f0;
    } else {
        out_meta->delta_f0 = 0.0f;
    }
    fe->prev_f0 = f0;
}
