/**
 * @file feature_extractor.h
 * @brief 音響特徴量 (F0, RMS, Energy, ΔF0) 抽出モジュール
 *
 * 【設計方針】
 * - ゼロ動的メモリ確保 (zero malloc)
 * - 16kHz サンプリングレート固定
 * - 軽量自己相関法によるピッチ抽出 (80Hz 〜 500Hz)
 */

#ifndef FEATURE_EXTRACTOR_H
#define FEATURE_EXTRACTOR_H

#include "ndwk_types.h"
#include <stddef.h>
#include <stdbool.h>

/**
 * @struct feature_extractor_t
 * @brief 特徴量抽出器の内部状態コンテキスト
 */
typedef struct {
    float prev_f0;             /**< 直前フレームの基本周波数 [Hz] */
    uint32_t silence_frames;   /**< 連続無音フレーム数 */
    uint64_t frame_index;      /**< 累積処理フレーム数 */
} feature_extractor_t;

/**
 * @brief 特徴量抽出器を初期化する
 * @param fe 対象コンテキスト
 */
void feature_extractor_init(feature_extractor_t *fe);

/**
 * @brief 1フレーム分の音声サンプルから音響特徴量を抽出しメタデータを構築する
 *
 * @param fe 対象コンテキスト
 * @param samples 16kHz float32 PCM サンプル配列
 * @param num_samples サンプル数 (例: 512)
 * @param is_speech VAD発話状態 (true: 発話中, false: 無音)
 * @param vad_prob VAD確信度スコア (0.0f〜1.0f)
 * @param token_id 直近確定トークンID (-1: トークンなし)
 * @param token_conf トークン確信度
 * @param out_meta 出力先メタデータ構造体ポインタ
 */
void feature_extractor_process_frame(
    feature_extractor_t *fe,
    const float *samples,
    size_t num_samples,
    bool is_speech,
    float vad_prob,
    int32_t token_id,
    float token_conf,
    ndwk_frame_meta_t *out_meta
);

#endif // FEATURE_EXTRACTOR_H
