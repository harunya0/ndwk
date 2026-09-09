#include "mic_reader.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
// miniaudio の実装マクロはプロジェクト全体でこのファイルだけで定義する
#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"
#define MIC_RING_BUFFER_SECONDS  2.0f // 2秒分のFIFOバッファ

struct mic_reader_t {
    ma_device device;
    bool is_started;
    unsigned int sample_rate;

    float *rb_buffer;
    size_t rb_capacity;
    size_t rb_write_pos;
    size_t rb_read_pos;
    size_t rb_count;
    pthread_mutex_t mutex;
};

static void on_audio_capture(ma_device *pDevice, void *pOutput, const void *pInput, ma_uint32 frameCount) {
    (void)pOutput; // 出力は使用しない
    mic_reader_t *mic = (mic_reader_t *)pDevice->pUserData;
    if (!mic || !pInput || frameCount == 0) return;

    const float *in_samples = (const float *)pInput;

    pthread_mutex_lock(&mic->mutex);

    for (ma_uint32 i = 0; i < frameCount; ++i) {
        mic->rb_buffer[mic->rb_write_pos] = in_samples[i];
        mic->rb_write_pos = (mic->rb_write_pos + 1) % mic->rb_capacity;

        if (mic->rb_count < mic->rb_capacity) {
            mic->rb_count++;
        } else {
            // バッファが満杯の場合は最古のデータを1つ捨てて read_pos を進める
            mic->rb_read_pos = (mic->rb_read_pos + 1) % mic->rb_capacity;
        }
    }
    pthread_mutex_unlock(&mic->mutex);
}

mic_reader_t *mic_reader_create(unsigned int sample_rate) {
    mic_reader_t *mic = malloc(sizeof(mic_reader_t));
    if (!mic) return NULL;
    memset(mic, 0, sizeof(*mic));

    mic->sample_rate = sample_rate;
    mic->rb_capacity = (size_t)(MIC_RING_BUFFER_SECONDS * sample_rate);
    mic->rb_buffer = malloc(mic->rb_capacity * sizeof(float));
    pthread_mutex_init(&mic->mutex, NULL);

    ma_device_config config = ma_device_config_init(ma_device_type_capture);
    config.capture.format = ma_format_f32;
    config.capture.channels = 1;
    config.sampleRate = sample_rate;
    config.dataCallback = on_audio_capture;
    config.pUserData = mic;

    if (ma_device_init(NULL, &config, &mic->device) != MA_SUCCESS) {
        free(mic->rb_buffer);
        pthread_mutex_destroy(&mic->mutex);
        free(mic);
        return NULL;
    }

    printf("[Mic] Opened capture device: %s\n", mic->device.capture.name);

    return mic;
}

void mic_reader_destroy(mic_reader_t *mic) {
    if (!mic) return;
    mic_reader_stop(mic);
    ma_device_uninit(&mic->device);
    pthread_mutex_destroy(&mic->mutex);
    free(mic->rb_buffer);
    free(mic);
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
    if (!mic || !out_samples || max_samples == 0) return 0;

    pthread_mutex_lock(&mic->mutex);

    // 要求されたサンプル数 (max_samples) が溜まるまでは読み出さない (ドロップ防止)
    if (mic->rb_count < max_samples) {
        pthread_mutex_unlock(&mic->mutex);
        return 0;
    }

    for (size_t i = 0; i < max_samples; i++) {
        out_samples[i] = mic->rb_buffer[mic->rb_read_pos];
        mic->rb_read_pos = (mic->rb_read_pos + 1) % mic->rb_capacity;
    }
    mic->rb_count -= max_samples;
    pthread_mutex_unlock(&mic->mutex);

    return max_samples;
}
