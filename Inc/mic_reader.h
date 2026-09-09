#ifndef MIC_READER_H
#define MIC_READER_H

#include <stddef.h>
#include <stdbool.h>

typedef struct mic_reader_t mic_reader_t;

mic_reader_t *mic_reader_create(unsigned int sample_rate);
void mic_reader_destroy(mic_reader_t *mic);

bool mic_reader_start(mic_reader_t *mic);
void mic_reader_stop(mic_reader_t *mic);

size_t mic_reader_read(mic_reader_t *mic, float *out_samples, size_t max_samples);

#endif // MIC_READER_H
