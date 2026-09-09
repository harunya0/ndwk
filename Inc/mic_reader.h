/**
 * @file mic_reader.h
 * @brief マイクリアルタイム録音・ロックフリー SPSC リングバッファ管理
 *
 * 【責務】
 * - `miniaudio` ライブラリを使用したクロスプラットフォームな低レイテンシマイク録音
 * - 音声キャプチャスレッドと推論メインスレッド間の排他制御をロックフリー (無ロック) で仲介
 * - C11 `<stdatomic.h>` と SIMD 64バイトアライメントによる超高速な単一生産者・単一消費者 (SPSC) キュー
 */

#ifndef MIC_READER_H
#define MIC_READER_H

#include <stddef.h>
#include <stdbool.h>

/**
 * @brief マイクリーダーの内部コンテキスト (不透明ポインタ)
 */
typedef struct mic_reader_t mic_reader_t;

/**
 * @brief マイクリーダーインスタンスを初期化・生成する
 *
 * @param sample_rate 録音サンプリング周波数 (Hz, 例: 16000)
 * @return mic_reader_t* 成功時はインスタンスポインタ、デバイス初期化失敗時は NULL
 */
mic_reader_t *mic_reader_create(unsigned int sample_rate);

/**
 * @brief マイクデバイスを停止し、リソースを解放する
 *
 * @param mic 対象のマイクリーダーインスタンス
 */
void mic_reader_destroy(mic_reader_t *mic);

/**
 * @brief マイクからの音声キャプチャを開始する
 *
 * @param mic 対象のマイクリーダーインスタンス
 * @return bool 開始成功時は true、失敗時は false
 */
bool mic_reader_start(mic_reader_t *mic);

/**
 * @brief マイクからの音声キャプチャを一時停止・停止する
 *
 * @param mic 対象のマイクリーダーインスタンス
 */
void mic_reader_stop(mic_reader_t *mic);

/**
 * @brief リングバッファから蓄積された PCM サンプルをノンブロッキングで読み出す
 *
 * @param mic 対象のマイクリーダーインスタンス
 * @param out_samples サンプル格納先の float32 配列
 * @param max_samples 読み出したいサンプル数 (例: NDWK_VAD_WINDOW_SIZE = 512)
 * @return size_t 実際に読み出されたサンプル数 (max_samples 分溜まっていなければ 0)
 */
size_t mic_reader_read(mic_reader_t *mic, float *out_samples, size_t max_samples);

#endif // MIC_READER_H
