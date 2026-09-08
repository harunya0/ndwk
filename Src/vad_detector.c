#include "vad_detector.h"
#include "model_config.h"
#include "sherpa-onnx/c-api/c-api.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

struct vad_detector_t {
    const SherpaOnnxVoiceActivityDetector *vad;
};

vad_detector_t *vad_detector_create(const char *models_dir) {
    vad_detector_t *detector = malloc(sizeof(vad_detector_t));
    if (!detector) return NULL;

    SherpaOnnxVadModelConfig config = model_config_create_vad(models_dir);
    detector->vad = SherpaOnnxCreateVoiceActivityDetector(&config, 30.0f);

    if (!detector->vad) {
        fprintf(stderr, "[vad_detector] Failed to create VAD\n");
        free(detector);
        return NULL;
    }
    return detector;
}

void vad_detector_destroy(vad_detector_t *detector) {
    if (detector) {
        if (detector->vad) {
            SherpaOnnxDestroyVoiceActivityDetector(detector->vad);
        }
        free(detector);
    }
}

void vad_detector_accept(vad_detector_t *detector, const float *samples, size_t num_samples) {
    if (!detector || !detector->vad || !samples) return;

    size_t offset = 0;
    while (offset < num_samples) {
        SherpaOnnxVoiceActivityDetectorAcceptWaveform(detector->vad, samples + offset, 512);
        offset += 512;
    }
}

void vad_detector_flush(vad_detector_t *detector) {
    if (detector && detector->vad) {
        SherpaOnnxVoiceActivityDetectorFlush(detector->vad);
    }
}

bool vad_detector_pop_segment(vad_detector_t *detector, vad_segment_t *out_seg) {
    if (!detector || !detector->vad || !out_seg) return false;

    if (SherpaOnnxVoiceActivityDetectorEmpty(detector->vad)) {
        return false;
    }

    const SherpaOnnxSpeechSegment *seg = SherpaOnnxVoiceActivityDetectorFront(detector->vad);
    out_seg->samples = seg->samples;
    out_seg->num_samples = (size_t)seg->n;

    SherpaOnnxVoiceActivityDetectorPop(detector->vad);

    return true;
}
