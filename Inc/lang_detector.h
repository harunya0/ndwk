/**
 * @file lang_detector.h
 * @brief Whisper Tiny による発話言語自動識別 (Spoken Language Identification: SLID)
 *
 * 【責務】
 * - VAD で検出された発話冒頭音声から、話されている言語 (日本語・英語・中国語・韓国語等) を推論
 * - 言語コード文字列 ("ja", "en" 等) と列挙型 `ndwk_lang_t` の相互変換
 * - 音声認識エンジン (ASR) を動的に切り替えるためのトリガーを提供
 */

#ifndef LANG_DETECTOR_H
#define LANG_DETECTOR_H

#include "ndwk_types.h"
#include <stddef.h>

/**
 * @brief 言語識別器の内部コンテキスト (不透明ポインタ)
 */
typedef struct lang_detector_t lang_detector_t;

/**
 * @brief 言語識別器インスタンスを生成・初期化する
 *
 * @param models_dir Whisper ONNX モデルが格納されているディレクトリ
 * @return lang_detector_t* 初期化されたインスタンスポインタ (失敗時は NULL)
 */
lang_detector_t *lang_detector_create(const char *models_dir);

/**
 * @brief 言語識別器を破棄し、リソースを解放する
 *
 * @param detector 対象インスタンスポインタ
 */
void lang_detector_destroy(lang_detector_t *detector);

/**
 * @brief 与えられた音声サンプルから言語を識別する
 *
 * @param detector 対象インスタンスポインタ
 * @param samples float32 PCM サンプル配列 (16kHz)
 * @param num_samples サンプル数 (内部で最大4秒分にトリミング)
 * @return ndwk_lang_t 識別された言語コード列挙値 (判別不能時はデフォルト NDWK_LANG_JA)
 */
ndwk_lang_t lang_detector_detect(lang_detector_t *detector, const float *samples, size_t num_samples);

/**
 * @brief 言語文字列 ("ja", "zh", "en", "ko") から列挙値を取得 (16bitビットパック高速判定)
 *
 * @param str 言語コード文字列
 * @return ndwk_lang_t 対応する列挙値
 */
ndwk_lang_t lang_from_string(const char *str);

/**
 * @brief 列挙値から言語コード文字列を取得
 *
 * @param lang 言語列挙値
 * @return const char* 静的文字列 ("ja", "zh", "en", "ko")
 */
const char *lang_to_string(ndwk_lang_t lang);

#endif // LANG_DETECTOR_H
