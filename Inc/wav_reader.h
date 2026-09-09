/**
 * @file wav_reader.h
 * @brief WAV ファイル読み込み・PCM float32 バッファ管理
 *
 * 【責務】
 * - dr_wav (シングルヘッダオーディオライブラリ) をラップし、WAV音声データを float32 形式でデコード
 * - 読み込んだ音声データのチャンネル数、サンプリングレート、総サンプル数の保持
 * - メモリの安全な解放
 */

#ifndef WAV_READER_H
#define WAV_READER_H

#include <stddef.h>

/**
 * @struct wav_data_t
 * @brief 読み込まれた WAV 音声データを保持する構造体
 */
typedef struct {
    float *samples;            /**< 32bit 浮動小数点 (float32, [-1.0, 1.0]) の PCM サンプル配列 */
    size_t num_samples;        /**< 総サンプル数 (フレーム数 × チャンネル数) */
    unsigned int sample_rate;  /**< サンプリング周波数 (Hz, 例: 16000) */
    unsigned int num_channels; /**< 音声チャンネル数 (モノラル: 1, ステレオ: 2) */
} wav_data_t;

/**
 * @brief WAV ファイルを読み込み、float32 PCM 配列として展開する
 *
 * @param filename 読み込む WAV ファイルのパス
 * @param wav_data 読み込み結果を格納する構造体ポインタ (呼び出し側で割り当て)
 * @return int 成功時は 0、失敗時は -1
 */
int wav_reader_read(const char *filename, wav_data_t *wav_data);

/**
 * @brief wav_reader_read で確保された PCM バッファを解放する
 *
 * @param wav_data 解放対象の構造体ポインタ
 */
void wav_reader_free(wav_data_t *wav_data);

#endif // WAV_READER_H
