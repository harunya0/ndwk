/**
 * @file ndwk.h
 * @brief ndwk 音声認識システム 公開APIヘッダ
 * - 設計方針: C99 標準に準拠し、組み込み環境でも利用可能な軽量ヘッダ
 * - ゼロメモリ動的確保 (zero malloc) を前提とした静的バッファ設計
 * - 依存ライブラリを最小化し、組み込み環境でも容易にビルド可能
 * - 入出力先（I/O）完全分離: 標準入出力やファイルI/Oに依存せず、ユーザー側で自由に入出力を実装可能
 * - 句読点復元エンジンは日本語専用であり、他言語では無効化される
 * - 多言語対応: 日本語、英語、中国語、韓国語に対応
 * - 多言語FFI: C/C++/Python/Go/RustなどのFFIを通じて、他言語からも利用可能
 */

#ifndef NDWK_H
#define NDWK_H

#include "ndwk_types.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief ndwk エンジンコンテキスト（不透明ポインタ）
 */
typedef struct ndwk_t ndwk_t;

/**
 * @brief 速報字幕 (Partial) 出力時のコールバック関数型
 * @param text 速報文字列 (ヌル終端)
 * @param user_data ユーザー登録の自由ポインタ
 */
typedef void (*ndwk_on_partial_cb)(const char *text, void *user_data);

/**
 * @brief 確定字幕 (Final) 出力時のコールバック関数型
 * @param lang 確定された発話の言語コード (NDWK_LANG_JA 等)
 * @param text 句読点復元済みの確定文字列 (ヌル終端)
 * @param user_data ユーザー登録の自由ポインタ
 */
typedef void (*ndwk_on_final_cb)(ndwk_lang_t lang, const char *text, void *user_data);

/**
 * @brief ndwk エンジン初期化設定構造体
 * 
 * すべての言語から直接編集可能な設定パラメータです。
 * 必ず最初に ndwk_default_config() で初期値を取得してから、必要な値だけ上書きしてください。
 */
typedef struct {
    /* --- 1. 基本設定 --- */
    const char *models_dir;          /**< モデル配置ディレクトリパス ("models") */
    ndwk_lang_t default_lang;        /**< デフォルト認識言語 (NDWK_LANG_JA 等) */
    bool auto_detect;                /**< 自動言語判別 (LID) を有効化するか */
    bool enable_punct;               /**< 日本語句読点復元エンジンを有効化するか */
    /* --- 2. VAD / 発話区間検出チューニング --- */
    float vad_threshold;             /**< 発話検知スコア閾値 (0.0〜1.0, デフォルト: 0.60f) */
    float vad_min_silence_sec;       /**< 発話確定とみなす無音継続時間 (秒, デフォルト: 0.50f) */
    float vad_min_speech_sec;        /**< 発話開始とみなす最小音声時間 (秒, デフォルト: 0.30f) */
    float vad_max_speech_sec;        /**< 連続発話を強制分割する最大時間 (秒, デフォルト: 8.00f) */
    /* --- 3. ストリーミング・パフォーマンス制御 --- */
    int32_t num_threads;             /**< ASR 推論スレッド数 (1, 2, 4 等, デフォルト: 2) */
    float partial_interval_sec;      /**< 速報推論・字幕更新間隔 (秒, デフォルト: 0.40f) */
    float partial_window_sec;        /**< 速報窓の最大音声長 (秒, デフォルト: 3.00f) */
    float preroll_sec;               /**< 文頭欠落防止のプリロール音声長 (秒, デフォルト: 0.50f) */
    /* --- 4. 出力コールバック --- */
    ndwk_on_partial_cb on_partial;   /**< 速報字幕コールバック (NULL可) */
    ndwk_on_final_cb   on_final;     /**< 確定字幕コールバック (NULL可) */
    void *user_data;                 /**< コールバックに引き渡されるユーザーポインタ (NULL可) */
} ndwk_config_t;

/**
 * @brief 最適化されたデフォルト設定を取得する
 * 
 * 内部で config.h に定義された黄金比パラメータがすべてセットされた構造体を返します。
 * 利用側はこの値を取得後、変更したいフィールドだけを上書きできます。
 */
ndwk_config_t ndwk_default_config(void);

/**
 * @brief ndwk 音声認識エンジンインスタンスを生成・初期化する
 * 
 * @param config 設定構造体ポインタ
 * @return ndwk_t* 初期化されたエンジンハンドル (失敗時は NULL)
 */
ndwk_t *ndwk_create(const ndwk_config_t *config);

/**
 * @brief ndwk エンジンを安全に破棄し、全リソースを解放する
 * 
 * @param engine 対象エンジンハンドル
 */
void ndwk_destroy(ndwk_t *engine);

/**
 * @brief 16kHz float32 音声サンプルをストリーミング投入する
 * 
 * 【使い方】
 * マイク録音コールバック、WAVファイル読み込み、ネットワークストリーム (WebSocket/RTP)、
 * またはマイコンの I2S DMA バッファ等から得られた音声データを任意のサイズで渡します。
 * 内部で VAD 検出・速報字幕生成・確定処理が自動で実行され、設定されたコールバックが発火します。
 * 
 * @param engine 対象エンジンハンドル
 * @param samples 16kHz float32 PCM 音声サンプル配列 (正規化範囲: -1.0 〜 +1.0)
 * @param num_samples サンプル数 (例: 512, 1024 等)
 */
void ndwk_feed_audio(ndwk_t *engine, const float *samples, size_t num_samples);

/**
 * @brief ストリーム終端 (EOF) を通知する
 * 
 * 音声ファイル末尾やマイク停止時に呼び出します。
 * VAD バッファに残っている未確定の音声セグメントを強制的に文字起こしして確定コールバックを呼びます。
 * 
 * @param engine 対象エンジンハンドル
 */
void ndwk_flush(ndwk_t *engine);


#ifdef __cplusplus
}
#endif

#endif // NDWK_H
