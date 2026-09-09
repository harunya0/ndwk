#include "audio_history.h"
#include "config.h"
#include <string.h>

#define AUDIO_HISTORY_SHIFT    18
#define AUDIO_HISTORY_CAPACITY (1 << AUDIO_HISTORY_SHIFT) // 262,144 samples (約16.38秒)
#define AUDIO_HISTORY_MASK     (AUDIO_HISTORY_CAPACITY - 1) // 0x3FFFF

struct audio_history_t {
    unsigned int sample_rate;
    size_t capacity;
    size_t size;             // 現在保持している有効サンプル数 (最大 CAPACITY)
    int64_t offset;          // 最古サンプルの絶対位置
    int64_t total_pushed;    // これまでに push された累計サンプル数
    int64_t last_seg_end;
    _Alignas(64) float buffer[AUDIO_HISTORY_CAPACITY];
    _Alignas(64) float work_buffer[AUDIO_HISTORY_CAPACITY];
};

static audio_history_t g_history;

static void ring_copy(const float *src, float *dst, int64_t start_pos, size_t count) {
    size_t start_idx = (size_t)(start_pos & AUDIO_HISTORY_MASK);
    size_t to_end = AUDIO_HISTORY_CAPACITY - start_idx;

    if (likely(to_end >= count)) {
        memcpy(dst, src + start_idx, count << NDWK_FLOAT_SHIFT);
    } else {
        memcpy(dst, src + start_idx, to_end << NDWK_FLOAT_SHIFT);
        memcpy(dst + to_end, src, (count - to_end) << NDWK_FLOAT_SHIFT);
    }
}

audio_history_t *audio_history_create(unsigned int sample_rate, float keep_seconds) {
    (void)keep_seconds;
    audio_history_t *h = &g_history;
    memset(h, 0, sizeof(*h));

    h->sample_rate = sample_rate;
    h->capacity = AUDIO_HISTORY_CAPACITY;
    return h;
}

void audio_history_push(audio_history_t *history, const float *samples, size_t num_samples) {
    if (unlikely(!history || !samples || num_samples == 0)) return;

    // バッファ容量を超える場合は、あふれる分を左にシフト（最古の音声を押し出す）
    if (unlikely(num_samples > history->capacity)) {
        samples += (num_samples - history->capacity);
        num_samples = history->capacity;
    }

    size_t write_idx = (size_t)(history->total_pushed & AUDIO_HISTORY_MASK);
    size_t to_end = AUDIO_HISTORY_CAPACITY - write_idx;

    if (likely(to_end >= num_samples)) {
        memcpy(history->buffer + write_idx, samples, num_samples << NDWK_FLOAT_SHIFT);
    } else {
        memcpy(history->buffer + write_idx, samples, to_end << NDWK_FLOAT_SHIFT);
        memcpy(history->buffer, samples + to_end, (num_samples - to_end) << NDWK_FLOAT_SHIFT);
    }
    
    history->total_pushed += (int64_t)num_samples;

    if (history->total_pushed > (int64_t)history->capacity) {
        history->offset = history->total_pushed - (int64_t)history->capacity;
        history->size = history->capacity;
    } else {
        history->offset = 0;
        history->size = (size_t)history->total_pushed;
    }
}

float *audio_history_with_preroll(
    audio_history_t *history,
    int64_t seg_start,
    const float *seg_samples,
    size_t seg_num_samples,
    size_t *out_num_samples
) {
    if (!history || !seg_samples || !out_num_samples) return NULL;

    int64_t want = seg_start - NDWK_PREROLL_SAMPLES;

    if (want < history->last_seg_end) want = history->last_seg_end;
    if (want < history->offset) want = history->offset;

    history->last_seg_end = seg_start + (int64_t)seg_num_samples;

    int64_t pre_count = 0;
    if (want < seg_start) {
        pre_count = seg_start - want;
    }

    size_t total_samples = (size_t)pre_count + seg_num_samples;
    if (total_samples > history->capacity) total_samples = history->capacity;

    float *out = history->work_buffer;

    if (pre_count > 0) {
        ring_copy(history->buffer, out, want, (size_t)pre_count);
    }
    memcpy(out + pre_count, seg_samples, seg_num_samples << NDWK_FLOAT_SHIFT);
    *out_num_samples = total_samples;
    return out;
}

const float *audio_history_get_recent(
    audio_history_t *history,
    size_t max_samples,
    size_t *out_samples
) {
    if (!history || history->size == 0 || !out_samples) return NULL;

    size_t count = history->size;
    if (count > max_samples) {
        count = max_samples;
    }

    int64_t start_pos = history->total_pushed - (int64_t)count;
    ring_copy(history->buffer, history->work_buffer, start_pos, count);

    *out_samples = count;
    return history->work_buffer;
}
