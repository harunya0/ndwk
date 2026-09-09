#include "vad_detector.h"
#include "model_config.h"
#include "sherpa-onnx/c-api/c-api.h"
#include "config.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

struct vad_detector_t {
    const SherpaOnnxVoiceActivityDetector *vad;
    const SherpaOnnxSpeechSegment *last_seg;
};

static vad_detector_t g_vad_detector;

vad_detector_t *vad_detector_create(const char *models_dir) {
    vad_detector_t *detector = &g_vad_detector;
    memset(detector, 0, sizeof(*detector));

    SherpaOnnxVadModelConfig config = model_config_create_vad(models_dir);
    detector->vad = SherpaOnnxCreateVoiceActivityDetector(&config, 30.0f);

    if (!detector->vad) {
        fprintf(stderr, "[vad_detector] Failed to create VAD\n");
        return NULL;
    }
    return detector;
}

void vad_detector_destroy(vad_detector_t *detector) {
    if (detector) {
        if (detector->last_seg) {
            SherpaOnnxDestroySpeechSegment(detector->last_seg);
            detector->last_seg = NULL;
        }
        if (detector->vad) {
            SherpaOnnxDestroyVoiceActivityDetector(detector->vad);
        }
    }
}

void vad_detector_accept(vad_detector_t *detector, const float *samples, size_t num_samples) {
    if (unlikely(!detector || !detector->vad || !samples)) return;

    if (likely(num_samples == NDWK_VAD_WINDOW_SIZE)) {
        SherpaOnnxVoiceActivityDetectorAcceptWaveform(detector->vad, samples, NDWK_VAD_WINDOW_SIZE);
        return;
    }

    size_t offset = 0;
    while (offset < num_samples) {
        SherpaOnnxVoiceActivityDetectorAcceptWaveform(detector->vad, samples + offset, NDWK_VAD_WINDOW_SIZE);
        offset += NDWK_VAD_WINDOW_SIZE;
    }
}

void vad_detector_flush(vad_detector_t *detector) {
    if (detector && detector->vad) {
        SherpaOnnxVoiceActivityDetectorFlush(detector->vad);
    }
}

bool vad_detector_pop_segment(vad_detector_t *detector, vad_segment_t *out_seg) {
    if (!detector || !detector->vad || !out_seg) return false;

    // 前回のセグメントがあれば安全に破棄
    if (detector->last_seg) {
        SherpaOnnxDestroySpeechSegment(detector->last_seg);
        detector->last_seg = NULL;
    }

    if (SherpaOnnxVoiceActivityDetectorEmpty(detector->vad)) {
        return false;
    }

    const SherpaOnnxSpeechSegment *seg = SherpaOnnxVoiceActivityDetectorFront(detector->vad);
    if (!seg) return false;

    out_seg->samples = seg->samples;
    out_seg->num_samples = (size_t)seg->n;
    out_seg->start_sample = (int64_t)seg->start;

    detector->last_seg = seg;
    SherpaOnnxVoiceActivityDetectorPop(detector->vad);

    return true;
}

bool vad_detector_is_speech(vad_detector_t *detector) {
    if (!detector || !detector->vad) return false;
    return SherpaOnnxVoiceActivityDetectorDetected(detector->vad) != 0;
}
