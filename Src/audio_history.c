#include "audio_history.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

struct audio_history_t {
    unsigned int sample_rate;
    size_t capacity;
    size_t size;
    int64_t offset;
    int64_t last_seg_end;
    float *buffer;
};

audio_history_t *audio_history_create(unsigned int sample_rate, float keep_seconds) {
    audio_history_t *h = malloc(sizeof(audio_history_t));
    if (!h) return NULL;

    h->sample_rate = sample_rate;
    h->capacity = (size_t)(keep_seconds * sample_rate);
    h->size = 0;
    h->offset = 0;
    h->last_seg_end = 0;

    h->buffer = malloc(h->capacity * sizeof(float));
    if (!h->buffer) {
        free(h);
        return NULL;
    }
    return h;
}

void audio_history_destroy(audio_history_t *history) {
    if (history) {
        free(history->buffer);
        free(history);
    }
}

void audio_history_push(audio_history_t *history, const float *samples, size_t num_samples) {
    if (!history || !samples || num_samples == 0) return;

    if (num_samples > history->capacity) {
        size_t overflow = (history->size + num_samples) - history->capacity;
        memmove(history->buffer, history->buffer + overflow, (history->size - overflow) * sizeof(float));
        history->offset += (int64_t)overflow;
        history->size -= overflow;
    }

    memcpy(history->buffer + history->size, samples, num_samples * sizeof(float));
    history->size += num_samples;
}

float *audio_history_with_preroll(
    audio_history_t *history,
    int64_t seg_start,
    const float *seg_samples,
    size_t seg_num_samples,
    size_t *out_num_samples
) {
    if (!history || !seg_samples || !out_num_samples) return NULL;

    int64_t preroll_samples = (int64_t)(1.0f * history->sample_rate); // 1 second of preroll
    int64_t want = seg_start - preroll_samples;

    if (want < history->last_seg_end) {
        want = history->last_seg_end;
    }
    if (want < history->offset) {
        want = history->offset;
    }

    history->last_seg_end = seg_start + (int64_t)seg_num_samples;

    int64_t pre_count = 0;
    if (want < seg_start) {
        pre_count = seg_start - want;
    }

    size_t total_samples = (size_t)pre_count + seg_num_samples;
    float *out = malloc(total_samples * sizeof(float));
    if (!out) return NULL;

    if (pre_count > 0) {
        size_t pre_offset = (size_t)(want - history->offset);
        memcpy(out, history->buffer + pre_offset, (size_t)pre_count * sizeof(float));
    }

    memcpy(out + pre_count, seg_samples, seg_num_samples * sizeof(float));

    *out_num_samples = total_samples;
    return out;
}
