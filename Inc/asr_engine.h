#ifndef ASR_ENGINE_H
#define ASR_ENGINE_H

#include "ndwk_types.h"
#include <stddef.h>

typedef struct asr_engine_t asr_engine_t;

asr_engine_t *asr_engine_create(const char *model_dir, ndwk_lang_t lang);
void asr_engine_destroy(asr_engine_t *engine);

const char *asr_engine_transcribe(const asr_engine_t *engine, const float *samples, size_t num_samples);

#endif // ASR_ENGINE_H
