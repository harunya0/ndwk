#ifndef AUDIO_HISTORY_H
#define AUDIO_HISTORY_H

#include <stddef.h>
#include <stdint.h>

typedef struct audio_history_t audio_history_t;

audio_history_t *audio_history_create(unsigned int sample_rate, float keep_seconds);

void audio_history_push(audio_history_t *history, const float *samples, size_t num_samples);

float *audio_history_with_preroll(
    audio_history_t *history,
    int64_t seg_start,
    const float *seg_samples,
    size_t seg_num_samples,
    size_t *out_num_samples
);

const float *audio_history_get_recent(
    audio_history_t *history, size_t max_samples, size_t *out_samples);

#endif // AUDIO_HISTORY_H
