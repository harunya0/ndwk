/**
 * @file model_config.h
 * @brief ONNX 音響モデル設定・ファイルパス構築ファクトリ
 *
 * 【責務】
 * - 各言語 (日本語・中国語・英語・韓国語) に特化した最先端 ASR モデルの ONNX 構成を構築
 * - Silero VAD (音声区間検出) モデルのチューニングパラメータ設定
 * - Sherpa-ONNX C-API 構造体へのパス・スレッド数・特徴抽出パラメータのマッピング
 */

#ifndef MODEL_CONFIG_H
#define MODEL_CONFIG_H

#include "ndwk_types.h"
#include "sherpa-onnx/c-api/c-api.h"

/**
 * @brief 指定した言語用の Sherpa-ONNX オフライン認識設定を構築する
 *
 * @param models_dir モデル配置ディレクトリパス
 * @param lang 対象言語コード (NDWK_LANG_JA, NDWK_LANG_ZH, etc.)
 * @return SherpaOnnxOfflineRecognizerConfig 構築された設定構造体
 */
SherpaOnnxOfflineRecognizerConfig model_config_create_asr(
    const char *models_dir, ndwk_lang_t lang);

/**
 * @brief Silero VAD (音声区間検出) 用の設定を構築する
 *
 * @param models_dir モデル配置ディレクトリパス
 * @return SherpaOnnxVadModelConfig 構築された VAD 設定構造体
 */
SherpaOnnxVadModelConfig model_config_create_vad(
    const char *models_dir);

#endif // MODEL_CONFIG_H
