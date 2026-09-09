/**
 * @file asr_engine.h
 * @brief 音声認識 (Automatic Speech Recognition: ASR) エンジンラッパー
 *
 * 【責務】
 * - Sherpa-ONNX のオフライン認識エンジンを管理し、音声サンプルから高精度な文字起こしを実行
 * - ReazonSpeech Zipformer, Paraformer, Parakeet TDT, SenseVoice の推論を統一インターフェースで隠蔽
 * - 認識結果文字列を内部バッファに安全にキャッシュし、呼び出し元の動的メモリ確保を不要化
 */

#ifndef ASR_ENGINE_H
#define ASR_ENGINE_H

#include "ndwk_types.h"
#include <stddef.h>

/**
 * @brief ASR エンジンの内部コンテキスト (不透明ポインタ)
 */
typedef struct asr_engine_t asr_engine_t;

/**
 * @brief 指定された言語用の ASR エンジンインスタンスを生成する
 *
 * @param model_dir ONNX モデル群が格納されているルートディレクトリ
 * @param lang 対象言語 (NDWK_LANG_JA, NDWK_LANG_EN 等)
 * @return asr_engine_t* 初期化されたインスタンスポインタ (失敗時は NULL)
 */
asr_engine_t *asr_engine_create(const char *model_dir, ndwk_lang_t lang);

/**
 * @brief ASR エンジンを破棄し、モデルのメモリを解放する
 *
 * @param engine 対象インスタンスポインタ
 */
void asr_engine_destroy(asr_engine_t *engine);

/**
 * @brief 音声サンプルを一括認識 (文字起こし) する
 *
 * @param engine 対象インスタンスポインタ
 * @param samples float32 PCM サンプル配列 (16kHz)
 * @param num_samples サンプル数
 * @return const char* 認識結果文字列 (内部静的バッファへのポインタ、free 不要)
 */
const char *asr_engine_transcribe(const asr_engine_t *engine, const float *samples, size_t num_samples);

#endif // ASR_ENGINE_H
