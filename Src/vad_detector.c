/**
 * @file vad_detector.c
 * @brief Silero VAD (ONNX) を用いた音声区間検出の実装
 *
 * 【アーキテクチャと処理フロー】
 * 1. Silero VAD テンソル仕様:
 *    - 入力フレーム長は 512 サンプル (16kHz で 32ms) 固定。
 *    - `vad_detector_accept` では 512 サンプルのホットパスを `likely` で最適化し、
 *      任意のバッファサイズにも対応できるよう 512 刻みのチャンクループを併装。
 *
 * 2. セグメントのライフサイクルとメモリ安全性:
 *    - Sherpa-ONNX の `SherpaOnnxSpeechSegment` は内部で動的メモリを持ちます。
 *    - `vad_detector_pop_segment` では、前回ポップしたセグメントを安全に破棄 (`last_seg`) してから
 *      新しいセグメントを取得することで、メモリリークを完全防止。
 *
 * 3. ゼロ malloc 設計:
 *    - VAD コンテキスト構造体 `vad_detector_t` は静的領域 `g_vad_detector` に確保。
 */

#include "vad_detector.h"
#include "model_config.h"
#include "sherpa-onnx/c-api/c-api.h"
#include "config.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

/* =========================================================================
 * 構造体定義
 * ========================================================================= */

struct vad_detector_t {
    const SherpaOnnxVoiceActivityDetector *vad; /**< Sherpa-ONNX VAD ハンドル */
    const SherpaOnnxSpeechSegment *last_seg;    /**< 前回ポップしたセグメントの破棄用保持ポインタ */
};

/**
 * @brief 静的コンテキスト領域 (ゼロ malloc)
 */
static vad_detector_t g_vad_detector;

/* =========================================================================
 * 公開関数
 * ========================================================================= */

vad_detector_t *vad_detector_create(const ndwk_config_t *cfg) {
    vad_detector_t *detector = &g_vad_detector;
    memset(detector, 0, sizeof(*detector));

    // Silero VAD のモデル設定を構築
    SherpaOnnxVadModelConfig config = model_config_create_vad(cfg);
    // 最大保持セグメントバッファ秒数: 30秒
    detector->vad = SherpaOnnxCreateVoiceActivityDetector(&config, 10.0f);

    if (!detector->vad) {
        fprintf(stderr, "[vad_detector] Error: Failed to create Silero VAD\n");
        return NULL;
    }
    return detector;
}

void vad_detector_destroy(vad_detector_t *detector) {
    if (detector) {
        if (detector->last_seg) {
            SherpaOnnxDestroySpeechSegment(detector->last_seg);
            detector->last_seg = NULL;
        }
        if (detector->vad) {
            SherpaOnnxDestroyVoiceActivityDetector(detector->vad);
            detector->vad = NULL;
        }
    }
}

void vad_detector_accept(vad_detector_t *detector, const float *samples, size_t num_samples) {
    if (unlikely(!detector || !detector->vad || !samples)) return;

    // 【ホットパス】標準チャンクサイズ (512サンプル = 32ms) の場合はループなしで即投入
    if (likely(num_samples == NDWK_VAD_WINDOW_SIZE)) {
        SherpaOnnxVoiceActivityDetectorAcceptWaveform(detector->vad, samples, NDWK_VAD_WINDOW_SIZE);
        return;
    }

    // 任意の長さのサンプルが渡された場合は 512 サンプルずつに分割して処理
    size_t offset = 0;
    while (offset < num_samples) {
        SherpaOnnxVoiceActivityDetectorAcceptWaveform(detector->vad, samples + offset, NDWK_VAD_WINDOW_SIZE);
        offset += NDWK_VAD_WINDOW_SIZE;
    }
}

void vad_detector_flush(vad_detector_t *detector) {
    if (detector && detector->vad) {
        SherpaOnnxVoiceActivityDetectorFlush(detector->vad);
    }
}

bool vad_detector_pop_segment(vad_detector_t *detector, vad_segment_t *out_seg) {
    if (!detector || !detector->vad || !out_seg) return false;

    // 前回のセグメントバッファが残っていれば安全に解放
    if (detector->last_seg) {
        SherpaOnnxDestroySpeechSegment(detector->last_seg);
        detector->last_seg = NULL;
    }

    // 確定した発話セグメントが存在するかチェック
    if (SherpaOnnxVoiceActivityDetectorEmpty(detector->vad)) {
        return false;
    }

    // キュー先頭のセグメント情報を取得
    const SherpaOnnxSpeechSegment *seg = SherpaOnnxVoiceActivityDetectorFront(detector->vad);
    if (!seg) return false;

    out_seg->samples = seg->samples;
    out_seg->num_samples = (size_t)seg->n;
    out_seg->start_sample = (int64_t)seg->start;

    // 次回ポップ時または破棄時に解放できるようにポインタを記憶
    detector->last_seg = seg;
    SherpaOnnxVoiceActivityDetectorPop(detector->vad);

    return true;
}

bool vad_detector_is_speech(vad_detector_t *detector) {
    if (!detector || !detector->vad) return false;
    return SherpaOnnxVoiceActivityDetectorDetected(detector->vad) != 0;
}
