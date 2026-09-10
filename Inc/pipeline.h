/**
 * @file pipeline.h
 * @brief リアルタイムストリーミング音声認識パイプライン統括ヘッダ
 *
 * 【責務】
 * - VAD (区間検出)、Audio History (リングバッファ)、SLID (言語識別)、
 *   ASR (音声認識)、Punctuation (句読点復元) の全モジュールを統合
 * - WAV ファイルによる実時間シミュレーション再生およびマイクライブ入力の実行管理
 * - 速報字幕 (Partial) と確定字幕 (Final) のリアルタイム出力制御
 */

#ifndef PIPELINE_H
#define PIPELINE_H

#include "ndwk_types.h"
#include "wav_reader.h"
#include <stdbool.h>

/**
 * @brief パイプライン管理コンテキスト (不透明ポインタ)
 */
typedef struct pipeline_t pipeline_t;

/**
 * @brief パイプラインインスタンスを生成・初期化する
 *
 * @param models_dir モデル配置ディレクトリパス ("models")
 * @param default_lang 起動時の初期言語 (NDWK_LANG_JA など)
 * @param auto_detect 発話言語の自動検出を有効にするか (true: Whisper Tiny SLID 併用)
 * @param enable_punct 句読点自動挿入を有効にするか (true: 有効)
 * @return pipeline_t* 初期化されたパイプラインポインタ (失敗時は NULL)
 */
pipeline_t *pipeline_create(const char *models_dir, ndwk_lang_t default_lang, bool auto_detect, bool enable_punct);

/**
 * @brief パイプラインを破棄し、内包する全サブモジュール (ASR, VAD, LID, Punct) を解放する
 *
 * @param pipeline 対象パイプラインポインタ
 */
void pipeline_destroy(pipeline_t *pipeline);

/**
 * @brief WAV ファイルを入力とし、実時間再生ペースに同期してストリーミング認識を実行する
 *
 * @param pipeline 対象パイプラインポインタ
 * @param wav 読み込み済みの WAV オーディオデータ構造体
 */
void pipeline_run_wav(pipeline_t *pipeline, const wav_data_t *wav);

/**
 * @brief マイクからのリアルタイム入力をキャプチャし、低遅延ストリーミング認識を実行する
 * - Ctrl+C (SIGINT) が押されるまでループ実行し、終了時に残音声をフラッシュして終了します。
 *
 * @param pipeline 対象パイプラインポインタ
 */
void pipeline_run_mic(pipeline_t *pipeline);

#endif // PIPELINE_H
