/**
 * @file pipeline.c
 * @brief リアルタイムストリーミング音声認識パイプラインのオーケストレーション実装
 *
 * 【アーキテクチャと制御フロー】
 * 1. サブモジュールの統合協調:
 *    - 入力音声を 512 サンプル (32ms) 刻みで `audio_history` (リングバッファ) と `vad_detector` に投入。
 *    - 発話中 (`vad_detector_is_speech`) は、一定間隔 (`NDWK_PARTIAL_INTERVAL_SAMPLES`) ごとに
 *      直近音声を取得して速報テキスト (`~ ...`) を画面更新。
 *    - 発話終了時、VAD がセグメントを出力 (`pop_segment`)。
 *      `audio_history_with_preroll` で冒頭音声 (1.0秒) を巻き戻し結合した上で、
 *      必要に応じて言語識別 (SLID) → ASR 本推論 → 句読点復元 (`punct_engine_restore`) を経て確定表示。
 *
 * 2. 整数ナノ秒クロック同期 (実時間シミュレーション):
 *    - WAV モードでは、オフセットから「本来あるべき経過時間」をナノ秒単位で計算:
 *      `audio_ns = offset * 62500LL` (16kHz のため 1 サンプル = 62,500 ナノ秒)。
 *    - `clock_gettime(CLOCK_MONOTONIC)` と比較し、処理が先行している場合は `nanosleep` で正確に待機。
 *      高価な浮動小数点除算を排除し、64bit 整数演算のみでジッターのない実時間同期を実現。
 *
 * 3. 差分テキスト更新による画面チラつき防止:
 *    - 速報字幕は、前回の出力文字列 `last_partial` と比較し、変化があった場合のみ
 *      ANSI エスケープシーケンス `\033[2K\r` で行を消去して再描画。
 *
 * 4. ゼロ malloc 設計:
 *    - パイプライン状態コンテキストは静的領域 `g_pipeline` に配置。
 */

#include "pipeline.h"
#include "config.h"
#include "model_config.h"
#include "vad_detector.h"
#include "asr_engine.h"
#include "audio_history.h"
#include "lang_detector.h"
#include "mic_reader.h"
#include "punct_engine.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <signal.h>

/* =========================================================================
 * 構造体定義
 * ========================================================================= */

/**
 * @struct pipeline_t
 * @brief パイプライン全体の状態・サブモジュールハンドルを保持する構造体
 */
struct pipeline_t {
    const char *models_dir;      /**< モデル配置ディレクトリパス */
    bool auto_detect;            /**< 言語自動判別有効フラグ */
    ndwk_lang_t current_lang;    /**< 現在アクティブな認識言語 */
    asr_engine_t *asr;           /**< 音声認識エンジン */
    vad_detector_t *vad;         /**< 音声区間検出器 */
    audio_history_t *history;    /**< 音声履歴リングバッファ */
    lang_detector_t *lid;        /**< 言語識別器 (Whisper Tiny SLID) */
    punct_engine_t *punct;       /**< 日本語句読点復元エンジン */
    char last_partial[512];      /**< 前回表示した速報テキスト (差分描画用キャッシュ) */
};

/**
 * @brief 静的パイプラインインスタンス (ゼロ malloc)
 */
static pipeline_t g_pipeline;

/* =========================================================================
 * ライフサイクル関数
 * ========================================================================= */

pipeline_t *pipeline_create(const char *models_dir, ndwk_lang_t default_lang, bool auto_detect) {
    pipeline_t *p = &g_pipeline;
    memset(p, 0, sizeof(pipeline_t));

    p->models_dir = models_dir;
    p->auto_detect = auto_detect;
    p->current_lang = default_lang;
    
    // サブモジュールの初期化
    p->vad = vad_detector_create(models_dir);
    p->history = audio_history_create(NDWK_SAMPLE_RATE, NDWK_HISTORY_KEEP_SEC);
    p->punct = punct_engine_create(models_dir);

    if (auto_detect) {
        p->lid = lang_detector_create(models_dir);
        // 初期状態としてデフォルト言語の ASR モデルをロード
        p->asr = asr_engine_create(models_dir, default_lang);
    } else {
        p->lid = NULL;
        p->asr = asr_engine_create(models_dir, default_lang);
    }

    // 必須モジュールの生成確認
    if (!p->asr || !p->vad || !p->history || (auto_detect && !p->lid)) {
        pipeline_destroy(p);
        return NULL;
    }

    return p;
}

void pipeline_destroy(pipeline_t *p) {
    if (!p) return;
    if (p->asr) asr_engine_destroy(p->asr);
    if (p->vad) vad_detector_destroy(p->vad);
    if (p->lid) lang_detector_destroy(p->lid);
    if (p->punct) punct_engine_destroy(p->punct);
}

/* =========================================================================
 * 内部処理: 発話確定セグメントの処理
 * ========================================================================= */

/**
 * @brief VAD から吐き出された確定セグメントを取り出し、言語判別・ASR・句読点復元を行って表示する
 */
static void pipeline_process_final_segment(pipeline_t *p) {
    vad_segment_t seg;
    while (vad_detector_pop_segment(p->vad, &seg)) {
        size_t full_samples = 0;
        // 冒頭欠落を防ぐため、1秒分の前置音声 (プリロール) を結合
        float *full_audio = audio_history_with_preroll(
                p->history, seg.start_sample, seg.samples, seg.num_samples, &full_samples);

        // 自動言語判別が有効な場合、必要に応じて ASR モデルを動的再ロード
        if (p->auto_detect && p->lid) {
            ndwk_lang_t detected = lang_detector_detect(p->lid, full_audio, full_samples);
            if (detected != p->current_lang) {
                asr_engine_destroy(p->asr);
                p->asr = asr_engine_create(p->models_dir, detected);
                p->current_lang = detected;
            }
        }

        // 確定音声全体の文字起こし
        if (p->asr) {
            const char *final_text = asr_engine_transcribe(p->asr, full_audio, full_samples);
            const char *display_text = final_text;

            // 日本語かつ句読点エンジンが有効な場合、自然な句読点「、」「。」「？」を復元
            if (p->current_lang == NDWK_LANG_JA && p->punct && final_text && final_text[0] != '\0') {
                display_text = punct_engine_restore(p->punct, final_text);
            }

            // 確定結果を出力 (速報行を上書きして改行)
            printf("\r[確定 (%s)]: %s\n", lang_to_string(p->current_lang), display_text);
            fflush(stdout);

            // 速報キャッシュをクリア
            p->last_partial[0] = '\0';
        }
    }
}

/* =========================================================================
 * ストリーミング実行: WAV ファイルモード (実時間シミュレーション)
 * ========================================================================= */

void pipeline_run_wav(pipeline_t *p, const wav_data_t *wav) {
    if (!p || !wav || !wav->samples) return;

    printf("=== ndwk Real-time Streaming Speech Recognition ===\n");

    size_t offset = 0;
    const size_t chunk_size = NDWK_VAD_WINDOW_SIZE; // 512 サンプル (32ms)
    size_t samples_since_partial = 0;

    // 実時間同期用のモノトニック基準時刻を取得
    struct timespec start_time;
    clock_gettime(CLOCK_MONOTONIC, &start_time);

    while (offset < wav->num_samples) {
        size_t n = chunk_size;
        if (offset + n > wav->num_samples) {
            n = wav->num_samples - offset;
        }

        // 音声を履歴リングバッファおよび VAD へ投入
        audio_history_push(p->history, wav->samples + offset, n);
        vad_detector_accept(p->vad, wav->samples + offset, n);

        offset += n;
        samples_since_partial += n;

        // 1. 発話中の速報表示 (一定間隔かつ発話中のみ推論を行い、負荷を抑制)
        if (samples_since_partial >= NDWK_PARTIAL_INTERVAL_SAMPLES && vad_detector_is_speech(p->vad)) {
            samples_since_partial = 0;

            size_t recent_n = 0;
            const float *recent_audio = audio_history_get_recent(p->history, NDWK_PARTIAL_WINDOW_SAMPLES, &recent_n);
            if (recent_audio && p->asr) {
                const char *partial_text = asr_engine_transcribe(p->asr, recent_audio, recent_n);
                if (partial_text && partial_text[0] != '\0') {
                    // テキストに変化があった場合のみ画面更新 (チラつき防止)
                    if (strcmp(partial_text, p->last_partial) != 0) {
                        printf("\033[2K\r~ %s", partial_text);
                        fflush(stdout);
                        strncpy(p->last_partial, partial_text, sizeof(p->last_partial) - 1);
                        p->last_partial[sizeof(p->last_partial) - 1] = '\0';
                    }
                }
            }
        }

        // 2. 発話終了時の確定表示
        pipeline_process_final_segment(p);

        // 3. 実時間クロック同期 (整数ナノ秒演算)
        struct timespec now;
        clock_gettime(CLOCK_MONOTONIC, &now);

        // 16kHz において 1 サンプル = 62,500 ナノ秒
        int64_t audio_ns = (int64_t)offset * NDWK_SAMPLE_TO_NS;
        int64_t real_ns  = (int64_t)(now.tv_sec - start_time.tv_sec) * 1000000000LL +
                           (int64_t)(now.tv_nsec - start_time.tv_nsec);

        // 音声の本来の経過時間より現実の時計が早い場合、余剰時間をスリープ
        if (audio_ns > real_ns) {
            int64_t diff_ns = audio_ns - real_ns;
            struct timespec req = {
                .tv_sec  = 0,
                .tv_nsec = diff_ns
            };
            nanosleep(&req, NULL);
        }
    }

    // 音声終了時に残っている未確定区間をフラッシュ
    vad_detector_flush(p->vad);
    pipeline_process_final_segment(p);
}

/* =========================================================================
 * ストリーミング実行: マイクリアルタイム入力モード
 * ========================================================================= */

/**
 * @brief マイクループ終了通知用フラグ (Ctrl+C ハンドラから変更)
 */
static volatile bool g_mic_running = true;

static void sigint_handler(int signum) {
    (void)signum;
    g_mic_running = false;
}

void pipeline_run_mic(pipeline_t *p) {
    if (!p) return;

    mic_reader_t *mic = mic_reader_create(NDWK_SAMPLE_RATE);
    if (!mic || !mic_reader_start(mic)) {
        fprintf(stderr, "[pipeline] Error: Failed to initialize microphone capture.\n");
        if (mic) mic_reader_destroy(mic);
        return;
    }

    g_mic_running = true;
    signal(SIGINT, sigint_handler);

    printf("=== ndwk Real-time Streaming Speech Recognition (Mic) ===\n");
    printf("Listening... Press Ctrl+C to stop.\n");

    const size_t chunk_size = NDWK_VAD_WINDOW_SIZE;
    float chunk[NDWK_VAD_WINDOW_SIZE];
    size_t samples_since_partial = 0;

    while (g_mic_running) {
        // ロックフリーリングバッファからサンプルを取得
        size_t n = mic_reader_read(mic, chunk, chunk_size);
        if (n == 0) {
            usleep(5000); // 5ms 待機して次のキャプチャデータを待つ
            continue;
        }

        audio_history_push(p->history, chunk, n);
        vad_detector_accept(p->vad, chunk, n);
        samples_since_partial += n;

        // 速報字幕更新
        if (samples_since_partial >= NDWK_PARTIAL_INTERVAL_SAMPLES && vad_detector_is_speech(p->vad)) {
            samples_since_partial = 0;

            size_t recent_n = 0;
            const float *recent_audio = audio_history_get_recent(p->history, NDWK_PARTIAL_WINDOW_SAMPLES, &recent_n);

            if (recent_audio && p->asr) {
                const char *partial_text = asr_engine_transcribe(p->asr, recent_audio, recent_n);
                if (partial_text && partial_text[0] != '\0') {
                    if (strcmp(partial_text, p->last_partial) != 0) {
                        printf("\033[2K\r~ %s", partial_text);
                        fflush(stdout);
                        strncpy(p->last_partial, partial_text, sizeof(p->last_partial) - 1);
                        p->last_partial[sizeof(p->last_partial) - 1] = '\0';
                    }
                }
            }
        }

        // 確定字幕更新
        pipeline_process_final_segment(p);
    }

    printf("\nStopping microphone capture...\n");

    // 終了時に未確定の音声が残っていれば flush して文字起こし
    vad_detector_flush(p->vad);
    pipeline_process_final_segment(p);

    mic_reader_stop(mic);
    mic_reader_destroy(mic);
}
