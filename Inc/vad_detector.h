/**
 * @file vad_detector.h
 * @brief 音声区間検出 (Voice Activity Detection: VAD) モジュール
 *
 * 【責務】
 * - Silero VAD (ONNX) モデルを利用して、ストリーミング音声から発話区間 (Speech Segment) を自動検出
 * - 息継ぎや無音時間を監視し、発話の開始位置 (タイムスタンプ) と終了を判定
 * - 発話終了時に確定セグメントを生成・切り出してパイプラインへ提供
 */

#ifndef VAD_DETECTOR_H
#define VAD_DETECTOR_H

#include "ndwk.h"
#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

/**
 * @brief VAD 検出器の内部コンテキスト (不透明ポインタ)
 */
typedef struct vad_detector_t vad_detector_t;

/**
 * @struct vad_segment_t
 * @brief VAD によって切り出された1つの確定発話セグメント
 */
typedef struct {
    const float *samples; /**< 発話区間の PCM float32 サンプル配列 */
    size_t num_samples;   /**< 発話区間のサンプル数 */
    int64_t start_sample; /**< 音声ストリーム全体における発話開始位置 (絶対サンプル番号) */
} vad_segment_t;

/**
 * @brief VAD 検出器インスタンスを生成・初期化する
 *
 * @param cfg 設定構造体ポインタ
 * @return vad_detector_t* 初期化されたインスタンスポインタ (失敗時は NULL)
 */
vad_detector_t *vad_detector_create(const ndwk_config_t *cfg);

/**
 * @brief VAD 検出器を破棄し、関連リソースを解放する
 *
 * @param detector 対象インスタンスポインタ
 */
void vad_detector_destroy(vad_detector_t *detector);

/**
 * @brief 新たな音声サンプルを VAD に投入し、発話状態を更新する
 *
 * @param detector 対象インスタンスポインタ
 * @param samples float32 PCM サンプル配列
 * @param num_samples サンプル数 (512サンプルの倍数を推奨)
 */
void vad_detector_accept(vad_detector_t *detector, const float *samples, size_t num_samples);

/**
 * @brief ストリーム終了時に未完了の発話区間を強制的にセグメントとして吐き出させる
 *
 * @param detector 対象インスタンスポインタ
 */
void vad_detector_flush(vad_detector_t *detector);

/**
 * @brief 確定した発話セグメントが存在する場合、先頭から1つ取り出す (ポップ)
 *
 * @param detector 対象インスタンスポインタ
 * @param out_seg 取り出されたセグメント情報を受け取る構造体ポインタ
 * @return bool セグメントが取得できた場合は true、未完了または空の場合は false
 */
bool vad_detector_pop_segment(vad_detector_t *detector, vad_segment_t *out_seg);

/**
 * @brief 直近の音声が「発話中」と判定されているかを取得する
 *
 * @param detector 対象インスタンスポインタ
 * @return bool 発話中であれば true、無音であれば false
 */
bool vad_detector_is_speech(vad_detector_t *detector);

#endif // VAD_DETECTOR_H
