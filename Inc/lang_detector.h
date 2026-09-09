#ifndef LANG_DETECTOR_H
#define LANG_DETECTOR_H

#include "ndwk_types.h"
#include <stddef.h>

typedef struct lang_detector_t lang_detector_t;

lang_detector_t *lang_detector_create(const char *models_dir);
void lang_detector_destroy(lang_detector_t *detector);

ndwk_lang_t lang_detector_detect(lang_detector_t *detector, const float *samples, size_t num_samples);

ndwk_lang_t lang_from_string(const char *str);
const char *lang_to_string(ndwk_lang_t lang);

#endif // LANG_DETECTOR_H
