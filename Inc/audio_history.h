/**
 * @file audio_history.h
 * @brief リアルタイム音声履歴リングバッファ ＆ プリロール結合モジュール
 *
 * 【責務】
 * - ストリーミング音声データを常時保持し、直近 N 秒間の音声を $O(1)$ で管理
 * - 従来の `memmove` によるスライド配列を完全廃止し、2の冪乗リングバッファでゼロコピー化
 * - VAD が検出した発話セグメントの直前（プリロール区間）を安全に巻き戻して結合
 * - 速報字幕 (Partial) 用に「直近 N サンプル」を切り出して提供
 */

#ifndef AUDIO_HISTORY_H
#define AUDIO_HISTORY_H

#include <stddef.h>
#include <stdint.h>

/**
 * @brief 音声履歴バッファの内部コンテキスト (不透明ポインタ)
 */
typedef struct audio_history_t audio_history_t;

/**
 * @brief 音声履歴バッファインスタンスを生成・初期化する
 *
 * @param sample_rate サンプリング周波数 (Hz, 例: 16000)
 * @param keep_seconds 保持目標秒数 (内部では高速化のため 2^18 = 16.38 秒固定)
 * @return audio_history_t* 初期化されたインスタンスポインタ
 */
audio_history_t *audio_history_create(unsigned int sample_rate, float keep_seconds);

/**
 * @brief 新しい音声サンプルを履歴バッファに追加 (プッシュ) する
 *
 * @param history 対象インスタンスポインタ
 * @param samples 追加する float32 PCM サンプル配列
 * @param num_samples サンプル数
 */
void audio_history_push(audio_history_t *history, const float *samples, size_t num_samples);

/**
 * @brief VAD 発話区間の冒頭にプリロール (前置音声) を結合した音声配列を取得する
 *
 * @param history 対象インスタンスポインタ
 * @param seg_start セグメント開始サンプル位置 (絶対サンプリングタイムライン)
 * @param seg_samples VAD から渡された発話音声サンプル配列
 * @param seg_num_samples 発話サンプルの長さ
 * @param out_num_samples プリロール結合後の総サンプル数を格納する出力ポインタ
 * @return float* プリロールが結合された内部ワークバッファへのポインタ (free 不要)
 */
float *audio_history_with_preroll(
    audio_history_t *history,
    int64_t seg_start,
    const float *seg_samples,
    size_t seg_num_samples,
    size_t *out_num_samples
);

/**
 * @brief 速報表示用に、直近の最新音声サンプル (最大 max_samples) を取得する
 *
 * @param history 対象インスタンスポインタ
 * @param max_samples 取得したい最大サンプル数
 * @param out_samples 実際に取得されたサンプル数を格納する出力ポインタ
 * @return const float* 連続配置された音声データへのポインタ (内部ワークバッファ)
 */
const float *audio_history_get_recent(
    audio_history_t *history, size_t max_samples, size_t *out_samples);

#endif // AUDIO_HISTORY_H
