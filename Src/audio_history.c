/**
 * @file audio_history.c
 * @brief O(1) 音声履歴リングバッファおよびプリロール結合の実装
 *
 * 【アーキテクチャとアルゴリズム】
 * 1. O(1) 循環リングバッファ (memmove の完全排除):
 *    - 従来のスライドウィンドウ実装では、毎フレーム最新サンプルを追加するたびに
 *      配列全体を `memmove` で左にずらしており、1フレームあたり数MBの不要なメモリアクセスが発生していました。
 *    - 本実装では $2^{18} = 262,144$ サンプル (16kHz で約 16.38 秒分) の静的リングバッファを採用。
 *    - 単調増加する 64bit 累計サンプルカウンタ `total_pushed` をビットマスク `& 0x3FFFF` で
 *      物理インデックスに写像することで、追加処理を $O(1)$ の定数時間で完結させます。
 *
 * 2. プリロール結合 (Preroll Stitching):
 *    - VAD が検出した発話開始位置 `seg_start` から `NDWK_PREROLL_SAMPLES` (1秒 = 16000 サンプル) を巻き戻します。
 *    - ただし、前回の発話終了位置 `last_seg_end` や、リングバッファの最古保持位置 `offset` を下回らないよう
 *      二重のガードを適用し、音声の重複認識や過去データの破壊を完全に防ぎます。
 *
 * 3. ゼロ malloc ワークバッファ (_Alignas(64)):
 *    - プリロール結合音声や直近の音声は、構造体に静的確保された `work_buffer` へ転送して返却します。
 *    - 動的メモリ確保 (`malloc/free`) は一切発生せず、キャッシュラインに整合した超高速な配列転送を実現します。
 */

#include "audio_history.h"
#include "config.h"
#include <string.h>

/* =========================================================================
 * 構造体定義
 * ========================================================================= */

struct audio_history_t {
    unsigned int sample_rate; /**< サンプリング周波数 (Hz) */
    size_t capacity;          /**< バッファ容量 (AUDIO_HISTORY_CAPACITY) */
    size_t size;              /**< 現在バッファ内に保持されている有効サンプル数 */
    int64_t offset;           /**< バッファ内に残っている最古サンプルの絶対タイムライン位置 */
    int64_t total_pushed;     /**< システム起動からこれまでに push された累計サンプル数 */
    int64_t last_seg_end;     /**< 前回確定したセグメントの終了サンプル位置 (巻き戻しガード用) */

    /**
     * @brief 循環リングバッファ本体 (262,144 サンプル)
     */
    _Alignas(64) float buffer[AUDIO_HISTORY_CAPACITY];

    /**
     * @brief 外部返却用の連続化ワークバッファ (プリロール結合・直近音声用)
     */
    _Alignas(64) float work_buffer[AUDIO_HISTORY_CAPACITY];
};

/**
 * @brief 静的コンテキストインスタンス (ゼロ malloc)
 */
static audio_history_t g_history;

/* =========================================================================
 * 内部ヘルパー関数
 * ========================================================================= */

/**
 * @brief リングバッファ上の任意の位置から連続した出力バッファへコピーする
 *
 * @param src リングバッファ配列
 * @param dst コピー先連続配列
 * @param start_pos 絶対サンプルタイムライン位置 (0 から単調増加)
 * @param count コピーするサンプル数
 */
static void ring_copy(const float *src, float *dst, int64_t start_pos, size_t count) {
    size_t start_idx = (size_t)(start_pos & AUDIO_HISTORY_MASK);
    size_t to_end = AUDIO_HISTORY_CAPACITY - start_idx;

    if (likely(to_end >= count)) {
        // リングバッファの境界を跨がない場合: 1回の memcpy
        memcpy(dst, src + start_idx, count << NDWK_FLOAT_SHIFT);
    } else {
        // リングバッファの境界を跨ぐ場合: 終端までコピー後、先頭 (0) から残りをコピー
        memcpy(dst, src + start_idx, to_end << NDWK_FLOAT_SHIFT);
        memcpy(dst + to_end, src, (count - to_end) << NDWK_FLOAT_SHIFT);
    }
}

/* =========================================================================
 * 公開関数
 * ========================================================================= */

audio_history_t *audio_history_create(unsigned int sample_rate, float keep_seconds) {
    (void)keep_seconds; // 高速ビット演算のため 2^18 容量で固定
    audio_history_t *h = &g_history;
    memset(h, 0, sizeof(*h));

    h->sample_rate = sample_rate;
    h->capacity = AUDIO_HISTORY_CAPACITY;
    return h;
}

void audio_history_push(audio_history_t *history, const float *samples, size_t num_samples) {
    if (unlikely(!history || !samples || num_samples == 0)) return;

    // 一度にプッシュされた量がバッファ容量を超える場合は、末尾最新分のみ保持
    if (unlikely(num_samples > history->capacity)) {
        samples += (num_samples - history->capacity);
        num_samples = history->capacity;
    }

    size_t write_idx = (size_t)(history->total_pushed & AUDIO_HISTORY_MASK);
    size_t to_end = AUDIO_HISTORY_CAPACITY - write_idx;

    if (likely(to_end >= num_samples)) {
        memcpy(history->buffer + write_idx, samples, num_samples << NDWK_FLOAT_SHIFT);
    } else {
        memcpy(history->buffer + write_idx, samples, to_end << NDWK_FLOAT_SHIFT);
        memcpy(history->buffer, samples + to_end, (num_samples - to_end) << NDWK_FLOAT_SHIFT);
    }
    
    history->total_pushed += (int64_t)num_samples;

    // バッファが満杯になった後は、最古位置 (offset) を自動的に押し出す
    if (history->total_pushed > (int64_t)history->capacity) {
        history->offset = history->total_pushed - (int64_t)history->capacity;
        history->size = history->capacity;
    } else {
        history->offset = 0;
        history->size = (size_t)history->total_pushed;
    }
}

float *audio_history_with_preroll(
    audio_history_t *history,
    int64_t seg_start,
    const float *seg_samples,
    size_t seg_num_samples,
    size_t preroll_samples,
    size_t *out_num_samples
) {
    if (!history || !seg_samples || !out_num_samples) return NULL;

    // プリロール希望位置 = 発話開始位置 - 1秒分 (16000サンプル)
    int64_t want = seg_start - (int64_t)preroll_samples;

    // ガード1: 前回の発話区間の末尾よりも前には巻き戻さない (二重認識防止)
    if (want < history->last_seg_end) want = history->last_seg_end;
    // ガード2: リングバッファが保持している最古サンプルより過去には巻き戻せない
    if (want < history->offset) want = history->offset;

    // 今回のセグメント終了位置を次回のために記録
    history->last_seg_end = seg_start + (int64_t)seg_num_samples;

    int64_t pre_count = 0;
    if (want < seg_start) {
        pre_count = seg_start - want;
    }

    size_t copy_seg = seg_num_samples;
    if ((size_t)pre_count + copy_seg > history->capacity) {
        copy_seg = history->capacity - (size_t)pre_count;
    }

    float *out = history->work_buffer;
    if (pre_count > 0) {
        ring_copy(history->buffer, out, want, (size_t)pre_count);
    }
    memcpy(out + pre_count, seg_samples, copy_seg << NDWK_FLOAT_SHIFT);

    *out_num_samples = (size_t)pre_count + copy_seg;
    return out;
}

const float *audio_history_get_recent(
    audio_history_t *history,
    size_t max_samples,
    size_t *out_samples
) {
    if (!history || history->size == 0 || !out_samples) return NULL;

    size_t count = history->size;
    if (count > max_samples) {
        count = max_samples;
    }

    // リングバッファの最新末尾から count サンプル分をワークバッファへ展開
    int64_t start_pos = history->total_pushed - (int64_t)count;
    ring_copy(history->buffer, history->work_buffer, start_pos, count);

    *out_samples = count;
    return history->work_buffer;
}
