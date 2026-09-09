/**
 * @file wav_reader.c
 * @brief WAV ファイルのデコードおよびメモリ管理の実装
 *
 * 【責務】
 * - `dr_wav` ライブラリを用いて RIFF/WAV フォーマット (PCM 16bit, 24bit, 32bit float等) を解析
 * - すべての音声を正規化された 32bit 浮動小数点 (float32, 範囲 [-1.0, 1.0]) に一括デコード
 * - パイプラインへの入力として、サンプリングレート・チャンネル数・サンプル数を抽出
 */

#include "wav_reader.h"
#include <stdio.h>
#include <string.h>

#define DR_WAV_IMPLEMENTATION
#include "dr_wav.h"

/**
 * @brief WAV ファイルを開き、全サンプルを float32 配列としてメモリに展開する
 *
 * @param filename 対象 WAV ファイルのパス (UTF-8 / ASCII)
 * @param wav_data デコードされたオーディオ情報を受け取る構造体ポインタ
 * @return int 成功時は 0、ファイルが見つからない・破損している場合は -1
 *
 * 【処理の流れ】
 * 1. 引数の NULL チェックおよび出力先構造体のゼロクリア
 * 2. drwav_open_file_and_read_pcm_frames_f32 による WAV ヘッダ解析と PCM 展開
 * 3. 展開されたサンプルポインタおよびメタデータ (サンプリングレート等) を wav_data に格納
 */
int wav_reader_read(const char *filename, wav_data_t *wav_data) {
    if (!filename || !wav_data) return -1;
    memset(wav_data, 0, sizeof(*wav_data));

    unsigned int channels = 0;
    unsigned int sample_rate = 0;
    drwav_uint64 total_frames = 0;

    // dr_wav の高レベルAPIを使用: 任意の量子化ビット深度 (16bit, 24bit等) から float32 へ自動変換
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

/**
 * @brief 展開された PCM オーディオバッファを解放する
 *
 * @param wav_data 解放対象の WAV データ構造体ポインタ
 */
void wav_reader_free(wav_data_t *wav_data) {
    if (wav_data && wav_data->samples) {
        drwav_free(wav_data->samples, NULL);
        wav_data->samples = NULL;
        wav_data->num_samples = 0;
    }
}
