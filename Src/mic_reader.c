/**
 * @file mic_reader.c
 * @brief 低レイテンシ録音 ＆ ロックフリー SPSC リングバッファ実装
 *
 * 【アーキテクチャと設計思想】
 * 1. ロックフリー (Lock-Free SPSC Queue):
 *    - 音声ハードウェアコールバック (OSの高優先度リアルタイムスレッド) と、
 *      推論を実行するメインスレッドの間で、一切の `pthread_mutex` やスピンロックを排除。
 *    - C11 の `<stdatomic.h>` (`memory_order_acquire` / `memory_order_release`) による
 *      CPU メモリバリアを用い、デッドロックや優先度逆転 (Priority Inversion) を根本から防止。
 *
 * 2. キャッシュライン最適化 (_Alignas(64)):
 *    - 循環配列バッファを 64 バイト境界にアライメント配置。
 *    - CPU の L1 キャッシュライン (x86_64 / ARM Cortex 共通で 64 バイト) に合致させ、
 *      フォルス・シェアリング (False Sharing) の排除と AVX2/NEON の整列転送を保証。
 *
 * 3. 2の冪乗サイズリングバッファ (32768 サンプル):
 *    - 容量: $2^{15} = 32768$ サンプル (16kHz で約 2.048 秒分)。
 *    - 除算 (`%`) を完全排除し、ビットマスク (`& 0x7FFF`) で折り返しを $O(1)$ 1サイクルで実行。
 *
 * 4. 2段階バルクコピー (Two-Phase memcpy):
 *    - リングバッファの終端をまたぐデータ転送は、ループによる1要素コピーではなく、
 *      終端までと先頭からの最大2回の `memcpy` に分割して SIMD バス帯域を最大活用。
 */

#include "mic_reader.h"
#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdatomic.h>

#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"

/* =========================================================================
 * 定数定義 (2の冪乗サイズ・ビットマスク)
 * ========================================================================= */

/**
 * @brief リングバッファ容量: 2^15 = 32768 サンプル (16kHz で約 2.048 秒)
 * - 瞬間的な推論スパイクによる読み出し遅延を吸収するのに十分な容量
 */
#define MIC_RB_SHIFT    15
#define MIC_RB_CAPACITY (1 << MIC_RB_SHIFT)
#define MIC_RB_MASK     (MIC_RB_CAPACITY - 1) // 0x7FFF

/* =========================================================================
 * 構造体定義
 * ========================================================================= */

/**
 * @struct mic_reader_t
 * @brief マイク録音デバイスと SPSC リングバッファの状態
 */
struct mic_reader_t {
    ma_device device;           /**< miniaudio のオーディオキャプチャデバイスハンドル */
    bool is_started;            /**< キャプチャ動作中フラグ */
    unsigned int sample_rate;   /**< サンプリングレート (Hz) */

    /**
     * @brief 循環音声サンプルバッファ (32768 samples = 128 KB)
     * - `_Alignas(64)` により CPU キャッシュライン境界に整列
     */
    _Alignas(64) float rb_buffer[MIC_RB_CAPACITY];

    /**
     * @brief 書込み位置 (head) と読出し位置 (tail)
     * - 単調増加カウンタとして管理し、マスク (`& MIC_RB_MASK`) で物理インデックスを算出
     */
    atomic_size_t head;         /**< 生産者 (コールバック) が進める書込み累計数 */
    atomic_size_t tail;         /**< 消費者 (メインスレッド) が進める読出し累計数 */
};

/**
 * @brief 静的コンテキスト領域 (ゼロ malloc 保証)
 */
static mic_reader_t g_mic;

/* =========================================================================
 * コールバック関数 (オーディオデバイススレッド・生産者)
 * ========================================================================= */

/**
 * @brief miniaudio からの音声キャプチャコールバック
 *
 * 【ロックフリー書込み手順】
 * 1. `head` は自スレッドのみ更新するため `memory_order_relaxed` でロード
 * 2. `tail` は消費者スレッドが進めるため `memory_order_acquire` で安全に同期
 * 3. バッファあふれ発生時は、クラッシュを防ぐため最古データを破棄 (tail を強制前進)
 * 4. リングバッファ終端の境界を考慮して 1回または2回の `memcpy` でバルク転送
 * 5. 転送完了後、`atomic_store_explicit(..., memory_order_release)` で `head` を更新し、
 *    メインスレッドに新しいデータが存在することを通知
 */
static void on_audio_capture(ma_device *pDevice, void *pOutput, const void *pInput, ma_uint32 frameCount) {
    (void)pOutput; // マイク録音のため再生出力バッファは不使用
    mic_reader_t *mic = (mic_reader_t *)pDevice->pUserData;
    if (unlikely(!mic || !pInput || frameCount == 0)) return;

    const float *in_samples = (const float *)pInput;

    size_t head = atomic_load_explicit(&mic->head, memory_order_relaxed);
    size_t tail = atomic_load_explicit(&mic->tail, memory_order_acquire);

    size_t occumulate = head - tail;
    // バッファ容量を超えるオーバーフローが発生した場合の安全措置
    if (unlikely(occumulate + frameCount > MIC_RB_CAPACITY)) {
        size_t overflow = (occumulate + frameCount) - MIC_RB_CAPACITY;
        atomic_store_explicit(&mic->tail, tail + overflow, memory_order_relaxed);
    }

    size_t write_idx = head & MIC_RB_MASK;
    size_t to_end = MIC_RB_CAPACITY - write_idx;

    if (likely(to_end >= frameCount)) {
        // 折り返しなし: 1回の memcpy で転送 (バイト数 = frameCount << 2)
        memcpy(mic->rb_buffer + write_idx, in_samples, frameCount << NDWK_FLOAT_SHIFT);
    } else {
        // 折り返しあり: バッファ終端まで書いた後、先頭 (0) から残りを書き込む
        memcpy(mic->rb_buffer + write_idx, in_samples, to_end << NDWK_FLOAT_SHIFT);
        memcpy(mic->rb_buffer, in_samples + to_end, (frameCount - to_end) << NDWK_FLOAT_SHIFT);
    }

    // 書込みデータをメモリにフラッシュしてから head を更新 (Release バリア)
    atomic_store_explicit(&mic->head, head + frameCount, memory_order_release);
}

/* =========================================================================
 * 公開関数
 * ========================================================================= */

mic_reader_t *mic_reader_create(unsigned int sample_rate) {
    mic_reader_t *mic = &g_mic;
    memset(mic, 0, sizeof(*mic));

    mic->sample_rate = sample_rate;
    atomic_init(&mic->head, 0);
    atomic_init(&mic->tail, 0);

    ma_device_config config = ma_device_config_init(ma_device_type_capture);
    config.capture.format = ma_format_f32;     // 32bit float 固定
    config.capture.channels = 1;              // モノラル固定
    config.sampleRate = sample_rate;          // 16000 Hz
    config.dataCallback = on_audio_capture;
    config.pUserData = mic;

    if (ma_device_init(NULL, &config, &mic->device) != MA_SUCCESS) {
        return NULL;
    }

    printf("[Mic] Opened capture device: %s\n", mic->device.capture.name);
    return mic;
}

void mic_reader_destroy(mic_reader_t *mic) {
    if (!mic) return;
    mic_reader_stop(mic);
    ma_device_uninit(&mic->device);
}

bool mic_reader_start(mic_reader_t *mic) {
    if (!mic || mic->is_started) return false;
    if (ma_device_start(&mic->device) != MA_SUCCESS) {
        fprintf(stderr, "[Mic] Failed to start capture device.\n");
        return false;
    }
    mic->is_started = true;
    return true;
}

void mic_reader_stop(mic_reader_t *mic) {
    if (!mic || !mic->is_started) return;
    ma_device_stop(&mic->device);
    mic->is_started = false;
}

/**
 * @brief リングバッファからサンプルを読み出す (メインスレッド・消費者)
 *
 * 【ロックフリー読出し手順】
 * 1. 生産者が更新した最新の `head` を `memory_order_acquire` で同期
 * 2. 要求サンプル数 (`max_samples`) が溜まるまで読み出しを行わず 0 を返却 (ドロップ防止)
 * 3. 折り返しを判定し、1回または2回の `memcpy` で呼び出し元のバッファへ高速転送
 * 4. 転送完了後、`atomic_store_explicit(..., memory_order_release)` で `tail` を更新
 */
size_t mic_reader_read(mic_reader_t *mic, float *out_samples, size_t max_samples) {
    if (unlikely(!mic || !out_samples || max_samples == 0)) return 0;

    // 生産者の書込みを同期 (Acquire バリア)
    size_t head = atomic_load_explicit(&mic->head, memory_order_acquire);
    size_t tail = atomic_load_explicit(&mic->tail, memory_order_relaxed);

    size_t occupied = head - tail;

    // 要求されたサンプル数 (max_samples) が蓄積されるまでは読み出さない
    if (unlikely(occupied < max_samples)) {
        return 0;
    }

    size_t read_idx = tail & MIC_RB_MASK;
    size_t to_end = MIC_RB_CAPACITY - read_idx;

    if (likely(to_end >= max_samples)) {
        // 折り返しなし: 1回の memcpy
        memcpy(out_samples, mic->rb_buffer + read_idx, max_samples << NDWK_FLOAT_SHIFT);
    } else {
        // 折り返しあり: 終端まで読んだ後、先頭 (0) から残りを読み出す
        memcpy(out_samples, mic->rb_buffer + read_idx, to_end << NDWK_FLOAT_SHIFT);
        memcpy(out_samples + to_end, mic->rb_buffer, (max_samples - to_end) << NDWK_FLOAT_SHIFT);
    }

    // 読出し完了を生産者スレッドに公開 (Release バリア)
    atomic_store_explicit(&mic->tail, tail + max_samples, memory_order_release);
    return max_samples;
}
