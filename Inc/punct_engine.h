/**
 * @file punct_engine.h
 * @brief 日本語句読点復元オーケストレータ (ONNX Runtime セッション管理)
 *
 * 【責務】
 * - mojicast-punct-onnx (`punct_bert.int8.onnx`) の ONNX Runtime セッションの初期化と推論実行
 * - トークナイザ (`punct_tokenizer`) とフォーマッタ (`punct_formatter`) の連携オーケストレーション
 * - 平文テキストを受け取り、自然な句読点「、」「。」「？」を復元したテキストを返却
 */

#ifndef PUNCT_ENGINE_H
#define PUNCT_ENGINE_H

#include <stdbool.h>

/**
 * @brief 句読点復元エンジンの内部コンテキスト (不透明ポインタ)
 */
typedef struct punct_engine_t punct_engine_t;

/**
 * @brief 句読点復元エンジンのインスタンスを生成・初期化する
 *
 * @param models_dir モデル配置ディレクトリパス ("models")
 * @return punct_engine_t* 初期化されたインスタンスポインタ (失敗時は NULL)
 */
punct_engine_t *punct_engine_create(const char *models_dir);

/**
 * @brief 平文テキストに自然な句読点「、」「。」「？」を復元・付与する
 *
 * @param engine 対象エンジンポインタ
 * @param text 句読点のない平文 (UTF-8)
 * @return const char* 句読点が付与されたテキスト (内部静的バッファへのポインタ、free 不要)
 */
const char *punct_engine_restore(punct_engine_t *engine, const char *text);

/**
 * @brief 句読点復元エンジンを破棄し、ONNX セッションおよびリソースを解放する
 *
 * @param engine 対象エンジンポインタ
 */
void punct_engine_destroy(punct_engine_t *engine);

#endif // PUNCT_ENGINE_H
