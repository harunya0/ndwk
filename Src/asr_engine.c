/**
 * @file asr_engine.c
 * @brief Sherpa-ONNX オフライン音声認識エンジンのラッパー実装
 *
 * 【アーキテクチャとライフサイクル】
 * 1. オフラインストリームの安全なスコープ管理:
 *    - 推論要求ごとに `SherpaOnnxCreateOfflineStream` で軽量コンテキストを生成。
 *    - 音声波形投入 (`AcceptWaveform`) → デコード実行 (`DecodeOfflineStream`) → 結果抽出 (`GetResult`)。
 *    - 推論直後にストリームおよび結果オブジェクトを確実に破棄し、ストリーミング中の累積メモリ消費をゼロ化。
 *
 * 2. ゼロ malloc 結果キャッシュ:
 *    - 認識されたテキストは、エンジン構造体に静的確保された `last_text[1024]` に安全にコピー (`memcpy`)。
 *    - 呼び出し側は文字列の `free()` やライフサイクル管理を一切意識することなく利用可能。
 */

#include "ndwk.h"
#include "asr_engine.h"
#include "model_config.h"
#include "sherpa-onnx/c-api/c-api.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

/* =========================================================================
 * 構造体定義
 * ========================================================================= */

struct asr_engine_t {
    const SherpaOnnxOfflineRecognizer *recognizer; /**< Sherpa-ONNX 認識器ハンドル */
    char last_text[1024];                          /**< 認識結果文字列キャッシュ (ゼロ malloc) */
};

/**
 * @brief 静的インスタンス領域 (ゼロ malloc)
 */
static asr_engine_t g_asr_engine;

/* =========================================================================
 * 公開関数
 * ========================================================================= */

asr_engine_t *asr_engine_create(const ndwk_config_t *cfg, ndwk_lang_t lang) {
    asr_engine_t *engine = &g_asr_engine;
    memset(engine, 0, sizeof(*engine));

    // 言語に応じた ASR モデル設定を構築
    SherpaOnnxOfflineRecognizerConfig config = model_config_create_asr(cfg, lang);
    engine->recognizer = SherpaOnnxCreateOfflineRecognizer(&config);

    if (!engine->recognizer) {
        fprintf(stderr, "[asr_engine] Error: Failed to create recognizer for lang %d\n", lang);
        return NULL;
    }
    return engine;
}

void asr_engine_destroy(asr_engine_t *engine) {
    if (engine && engine->recognizer) {
        SherpaOnnxDestroyOfflineRecognizer(engine->recognizer);
        engine->recognizer = NULL;
    }
}

const char *asr_engine_transcribe(const asr_engine_t *engine, const float *samples, size_t num_samples) {
    if (!engine || !engine->recognizer || !samples || num_samples == 0) {
        return "";
    }
    
    // 認識用の一時オフラインストリームを生成
    const SherpaOnnxOfflineStream *stream = SherpaOnnxCreateOfflineStream(engine->recognizer);
    if (!stream) {
        return "";
    }

    // 16kHz PCM 音声サンプルを投入してデコードを実行
    SherpaOnnxAcceptWaveformOffline(stream, 16000, samples, (int32_t)num_samples);
    SherpaOnnxDecodeOfflineStream(engine->recognizer, stream);

    // 認識結果の文字列を取得
    const SherpaOnnxOfflineRecognizerResult *result = SherpaOnnxGetOfflineStreamResult(stream);

    asr_engine_t *mutable_engine = (asr_engine_t *)engine;
    if (result && result->text) {
        size_t len = strlen(result->text);
        if (len >= sizeof(mutable_engine->last_text)) {
            len = sizeof(mutable_engine->last_text) - 1; // バッファあふれ防止ガード
        }
        memcpy(mutable_engine->last_text, result->text, len);
        mutable_engine->last_text[len] = '\0';
    } else {
        mutable_engine->last_text[0] = '\0';
    }

    // ストリームおよび認識結果オブジェクトを確実に解放
    SherpaOnnxDestroyOfflineRecognizerResult(result);
    SherpaOnnxDestroyOfflineStream(stream);

    return mutable_engine->last_text;
}
