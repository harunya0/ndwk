#include "mic_reader.h"
#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdatomic.h>
#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"
#define MIC_RB_SHIFT    15
#define MIC_RB_CAPACITY (1 << MIC_RB_SHIFT) // 32768 samples
#define MIC_RB_MASK     (MIC_RB_CAPACITY - 1) // 0x7FFF

struct mic_reader_t {
    ma_device device;
    bool is_started;
    unsigned int sample_rate;

    _Alignas(64) float rb_buffer[MIC_RB_CAPACITY];
    atomic_size_t head;
    atomic_size_t tail;
};

static mic_reader_t g_mic;

static void on_audio_capture(ma_device *pDevice, void *pOutput, const void *pInput, ma_uint32 frameCount) {
    (void)pOutput; // 出力は使用しない
    mic_reader_t *mic = (mic_reader_t *)pDevice->pUserData;
    if (unlikely(!mic || !pInput || frameCount == 0)) return;

    const float *in_samples = (const float *)pInput;

    size_t head = atomic_load_explicit(&mic->head, memory_order_relaxed);
    size_t tail = atomic_load_explicit(&mic->tail, memory_order_acquire);

    size_t occumulate = head - tail;
    if (unlikely(occumulate + frameCount > MIC_RB_CAPACITY)) {
        size_t overflow = (occumulate + frameCount) - MIC_RB_CAPACITY;
        atomic_store_explicit(&mic->tail, tail + overflow, memory_order_relaxed);
    }

    size_t write_idx = head & MIC_RB_MASK;
    size_t to_end = MIC_RB_CAPACITY - write_idx;

    if (likely(to_end >= frameCount)) {
        memcpy(mic->rb_buffer + write_idx, in_samples, frameCount << NDWK_FLOAT_SHIFT);
    } else {
        memcpy(mic->rb_buffer + write_idx, in_samples, to_end << NDWK_FLOAT_SHIFT);
        memcpy(mic->rb_buffer, in_samples + to_end, (frameCount - to_end) << NDWK_FLOAT_SHIFT);
    }

    atomic_store_explicit(&mic->head, head + frameCount, memory_order_release);
}

mic_reader_t *mic_reader_create(unsigned int sample_rate) {
    mic_reader_t *mic = &g_mic;
    memset(mic, 0, sizeof(*mic));

    mic->sample_rate = sample_rate;
    atomic_init(&mic->head, 0);
    atomic_init(&mic->tail, 0);

    ma_device_config config = ma_device_config_init(ma_device_type_capture);
    config.capture.format = ma_format_f32;
    config.capture.channels = 1;
    config.sampleRate = sample_rate;
    config.dataCallback = on_audio_capture;
    config.pUserData = mic;

    if (ma_device_init(NULL, &config, &mic->device) != MA_SUCCESS) {
        return NULL;
    }

    printf("[Mic] Opened capture device: %s\n", mic->device.capture.name);

    return mic;
}

void mic_reader_destroy(mic_reader_t *mic) {
    if (!mic) return;
    mic_reader_stop(mic);
    ma_device_uninit(&mic->device);
}

bool mic_reader_start(mic_reader_t *mic) {
    if (!mic || mic->is_started) return false;
    if (ma_device_start(&mic->device) != MA_SUCCESS) {
        fprintf(stderr, "Failed to start microphone capture device.\n");
        return false;
    }
    mic->is_started = true;
    return true;
}

void mic_reader_stop(mic_reader_t *mic) {
    if (!mic || !mic->is_started) return;
    ma_device_stop(&mic->device);
    mic->is_started = false;
}

size_t mic_reader_read(mic_reader_t *mic, float *out_samples, size_t max_samples) {
    if (unlikely(!mic || !out_samples || max_samples == 0)) return 0;

    size_t head = atomic_load_explicit(&mic->head, memory_order_acquire);
    size_t tail = atomic_load_explicit(&mic->tail, memory_order_relaxed);

    size_t occupied = head - tail;

    // 要求されたサンプル数 (max_samples) が溜まるまでは読み出さない (ドロップ防止)
    if (unlikely(occupied < max_samples)) {
        return 0;
    }

    size_t read_idx = tail & MIC_RB_MASK;
    size_t to_end = MIC_RB_CAPACITY - read_idx;

    if (likely(to_end >= max_samples)) {
        memcpy(out_samples, mic->rb_buffer + read_idx, max_samples << NDWK_FLOAT_SHIFT);
    } else {
        memcpy(out_samples, mic->rb_buffer + read_idx, to_end << NDWK_FLOAT_SHIFT);
        memcpy(out_samples + to_end, mic->rb_buffer, (max_samples - to_end) << NDWK_FLOAT_SHIFT);
    }

    atomic_store_explicit(&mic->tail, tail + max_samples, memory_order_release);
    return max_samples;
}
