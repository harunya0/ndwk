#ifndef VAD_DETECTOR_H
#define VAD_DETECTOR_H

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

typedef struct vad_detector_t vad_detector_t;

typedef struct {
    const float *samples; // PCMデータのサンプル
    size_t num_samples;   // サンプル数
    int64_t start_sample; // セグメントの開始サンプル位置
} vad_segment_t;

vad_detector_t *vad_detector_create(const char *model_dir);
void vad_detector_destroy(vad_detector_t *detector);

void vad_detector_accept(vad_detector_t *detector, const float *samples, size_t num_samples);
void vad_detector_flush(vad_detector_t *detector);

bool vad_detector_pop_segment(vad_detector_t *detector, vad_segment_t *out_seg);

#endif // VAD_DETECTOR_H
