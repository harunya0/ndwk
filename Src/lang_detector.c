/**
 * @file lang_detector.c
 * @brief Whisper Tiny SLID による多言語自動識別実装
 *
 * 【アーキテクチャと最適化】
 * 1. 16bit ビットパック比較 (strcmp の完全排除):
 *    - 言語コードは "ja", "en", "zh", "ko" などの 2文字 ASCII。
 *    - マクロ `LANG_CODE(c1, c2)` により、2文字を 16bit 整数 (`uint16_t`) にパッキング。
 *    - 4分岐の `strcmp` 関数呼び出しを、CPU の 1 サイクル `switch` ジャンプテーブルに置換。
 *
 * 2. 推論窓の制限 (NDWK_LID_MAX_SAMPLES = 64000 = 4.0秒):
 *    - 言語識別は文頭の数秒で十分な統計情報が得られます。
 *    - 10秒以上の長い音声が渡された場合でも冒頭 4 秒分のみにスライスして推論することで、
 *      Whisper Tiny の推論遅延を最小化します。
 *
 * 3. ゼロ malloc 設計:
 *    - 静的コンテキスト `g_lang_detector` を使用。
 */

#include "lang_detector.h"
#include "sherpa-onnx/c-api/c-api.h"
#include "config.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

/**
 * @brief 2文字の ASCII コードを 16bit リトルエンディアン整数にパックするマクロ
 */
#define LANG_CODE(c1, c2) ((uint16_t)(c1) | ((uint16_t)(c2) << 8))

/* =========================================================================
 * 構造体定義
 * ========================================================================= */

struct lang_detector_t {
    const SherpaOnnxSpokenLanguageIdentification *slid; /**< Sherpa-ONNX SLID ハンドル */
};

/**
 * @brief 静的コンテキスト領域 (ゼロ malloc)
 */
static lang_detector_t g_lang_detector;

/* =========================================================================
 * 文字列変換ヘルパー (16bit ビットパック最適化)
 * ========================================================================= */

ndwk_lang_t lang_from_string(const char *str) {
    if (!str || str[0] == '\0' || str[1] == '\0') return NDWK_LANG_JA;

    // 先頭2文字を 16bit 整数として読み込み
    uint16_t code = (uint8_t)str[0] | ((uint16_t)(uint8_t)str[1] << 8);
    switch (code) {
        case LANG_CODE('j', 'a'): return NDWK_LANG_JA;
        case LANG_CODE('z', 'h'): return NDWK_LANG_ZH;
        case LANG_CODE('e', 'n'): return NDWK_LANG_EN;
        case LANG_CODE('k', 'o'): return NDWK_LANG_KO;
        default:                  return NDWK_LANG_JA;
    }
}

const char *lang_to_string(ndwk_lang_t lang) {
    switch (lang) {
        case NDWK_LANG_JA: return "ja";
        case NDWK_LANG_ZH: return "zh";
        case NDWK_LANG_EN: return "en";
        case NDWK_LANG_KO: return "ko";
        default:           return "ja";
    }
}

/* =========================================================================
 * 公開関数
 * ========================================================================= */

lang_detector_t *lang_detector_create(const char *models_dir) {
    lang_detector_t *d = &g_lang_detector;
    memset(d, 0, sizeof(*d));

    // Whisper Tiny のエンコーダ・デコーダ ONNX パスを設定
    static char enc[512], dec[512];
    snprintf(enc, sizeof(enc), "%s/sherpa-onnx-whisper-tiny/tiny-encoder.int8.onnx", models_dir);
    snprintf(dec, sizeof(dec), "%s/sherpa-onnx-whisper-tiny/tiny-decoder.int8.onnx", models_dir);

    SherpaOnnxSpokenLanguageIdentificationConfig config;
    memset(&config, 0, sizeof(config));
    config.whisper.encoder = enc;
    config.whisper.decoder = dec;
    config.num_threads = 2; // 言語判定は軽量モデルのため 2 スレッドで十分高速
    config.provider = "cpu";

    d->slid = SherpaOnnxCreateSpokenLanguageIdentification(&config);
    if (!d->slid) {
        fprintf(stderr, "[lang_detector] Error: Failed to create SLID instance\n");
        return NULL;
    }
    return d;
}

void lang_detector_destroy(lang_detector_t *detector) {
    if (detector && detector->slid) {
        SherpaOnnxDestroySpokenLanguageIdentification(detector->slid);
        detector->slid = NULL;
    }
}

ndwk_lang_t lang_detector_detect(lang_detector_t *detector, const float *samples, size_t num_samples) {
    if (!detector || !detector->slid || !samples || num_samples == 0) return NDWK_LANG_JA;

    SherpaOnnxOfflineStream *stream =
            SherpaOnnxSpokenLanguageIdentificationCreateOfflineStream(detector->slid);
    
    // 最大 4.0 秒 (64000 サンプル) に制限して推論を高速化
    size_t feed_samples = num_samples;
    if (feed_samples > NDWK_LID_MAX_SAMPLES) {
        feed_samples = NDWK_LID_MAX_SAMPLES;
    }

    SherpaOnnxAcceptWaveformOffline(stream, 16000, samples, (int32_t)feed_samples); 

    const SherpaOnnxSpokenLanguageIdentificationResult *res =
            SherpaOnnxSpokenLanguageIdentificationCompute(detector->slid, stream);

    ndwk_lang_t lang = NDWK_LANG_JA;
    if (res && res->lang) {
        lang = lang_from_string(res->lang);
    }

    SherpaOnnxDestroySpokenLanguageIdentificationResult(res);
    SherpaOnnxDestroyOfflineStream(stream);

    return lang;
}
