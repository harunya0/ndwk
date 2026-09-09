#ifndef PIPELINE_H
#define PIPELINE_H

#include "ndwk_types.h"
#include "wav_reader.h"
#include <stdbool.h>

typedef struct pipeline_t pipeline_t;

pipeline_t *pipeline_create(const char *models_dir, ndwk_lang_t default_lang, bool auto_detect);
void pipeline_destroy(pipeline_t *pipeline);

void pipeline_run_wav(pipeline_t *pipeline, const wav_data_t *wav);

#endif // PIPELINE_H
