#include "wav_reader.h"
#include <stdio.h>
#include <string.h>
#define DR_WAV_IMPLEMENTATION
#include "dr_wav.h"

int wav_reader_read(const char *filename, wav_data_t *wav_data) {
    if (!filename || !wav_data) return -1;
    memset(wav_data, 0, sizeof(*wav_data));

    unsigned int channels = 0;
    unsigned int sample_rate = 0;
    drwav_uint64 total_frames = 0;

    float *samples = drwav_open_file_and_read_pcm_frames_f32(
        filename, &channels, &sample_rate, &total_frames, NULL);

    if (!samples) {
        fprintf(stderr, "[wav_reader] Error: Failed to open %s\n", filename);
        return -1;
    }

    wav_data->samples = samples;
    wav_data->num_samples = (size_t)total_frames;
    wav_data->sample_rate = sample_rate;
    wav_data->num_channels = channels;
    return 0;
}

void wav_reader_free(wav_data_t *wav_data) {
    if (wav_data && wav_data->samples) {
        drwav_free(wav_data->samples, NULL);
        wav_data->samples = NULL;
        wav_data->num_samples = 0;
    }
}
