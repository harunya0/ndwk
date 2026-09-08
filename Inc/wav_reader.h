#ifndef WAV_READER_H
#define WAV_READER_H

#include <stddef.h>

typedef struct {
    float *samples; // PCMデータのサンプル
    size_t num_samples; // サンプル数
    unsigned int sample_rate; // サンプリングレート
    unsigned int num_channels; // チャンネル数
} wav_data_t;

// WAVファイルを読み込む関数(成功：0、失敗：-1)
int wav_reader_read(const char *filename, wav_data_t *wav_data);
void wav_reader_free(wav_data_t *wav_data);

#endif // WAV_READER_H
