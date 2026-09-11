#include <malloc.h>
/**
 * @file main.c
 * @brief ndwk 音声認識システムの CLI エントリポイント
 *
 * 【コマンドライン引数フォーマット】
 * 1. WAV ファイル入力モード:
 *    ./build/ndwk <WAVパス> [言語コード|auto]
 *    例: ./build/ndwk test/ja/ja_024.wav ja
 *    例: ./build/ndwk test/en/en_001.wav en
 *    例: ./build/ndwk test/ja/ja_024.wav auto
 *
 * 2. リアルタイムマイク入力モード:
 *    ./build/ndwk --mic [言語コード|auto]
 *    例: ./build/ndwk --mic ja
 *    例: ./build/ndwk --mic auto
 *
 * 3. 引数なし:
 *    デフォルトのテスト音声 (test/ja/ja_014.wav) を自動認識
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <time.h>
#include <unistd.h>

#include "ndwk.h"
#include "config.h"
#include "wav_reader.h"
#include "mic_reader.h"
#include "lang_detector.h"

/* =========================================================================== 
 * CLI用コールバック関数
 * ========================================================================= */

static void cli_on_partial(const char *text, void *user_data) {
    (void)user_data;
    printf("\033[2K\r~ %s", text);
    fflush(stdout);
}

static void cli_on_final(ndwk_lang_t lang, const char *text, void *user_data) {
    (void)user_data;
    printf("\r[確定(%s)]: %s\n", lang_to_string(lang), text);
    fflush(stdout);
}

 /* =========================================================================== 
 * wav入力ストリーミング
 * ========================================================================= */

static void run_wav(ndwk_t *engine, const char *wav_path) {
    wav_data_t wav;
    if (wav_reader_read(wav_path, &wav) != 0) {
        fprintf(stderr, "Error: Failed to read WAV file: %s\n", wav_path);
        return;
    }

    printf("=== ndwk Real-time Streaming (WAV) ===\n");

    const size_t chunk_size = NDWK_VAD_WINDOW_SIZE;
    size_t offset = 0;
    struct timespec start_time;
    clock_gettime(CLOCK_MONOTONIC, &start_time);

    while (offset < wav.num_samples) {
        size_t n = chunk_size;
        if (offset + n > wav.num_samples) {
            n = wav.num_samples - offset;
        }

        ndwk_feed_audio(engine, wav.samples + offset, n);
        offset += n;

        // リアルタイム再生速度に合わせるためのスリープ
        struct timespec current_time;
        clock_gettime(CLOCK_MONOTONIC, &current_time);
        int64_t audio_ns = (int64_t)offset * NDWK_SAMPLE_TO_NS;
        int64_t real_ns = (int64_t)(current_time.tv_sec - start_time.tv_sec) * 1000000000LL +
                          (current_time.tv_nsec - start_time.tv_nsec);
        if (audio_ns > real_ns) {
            struct timespec req = { .tv_sec = 0, .tv_nsec = audio_ns - real_ns };
            nanosleep(&req, NULL);
        }
    }
    ndwk_flush(engine);
    wav_reader_free(&wav);
}

/* ========================================================================== 
 * マイク入力ストリーミング
 * ========================================================================= */

static volatile bool g_mic_running = true;

static void sigint_handler(int signum) {
    (void)signum;
    g_mic_running = false;
}

static void run_mic(ndwk_t *engine) {
    mic_reader_t *mic = mic_reader_create(NDWK_SAMPLE_RATE);
    if (!mic || !mic_reader_start(mic)) {
        fprintf(stderr, "Error: Failed to start microphone reader\n");
        if (mic) mic_reader_destroy(mic);
        return;
    }

    g_mic_running = true;
    signal(SIGINT, sigint_handler);

    printf("=== ndwk Real-time Streaming (Microphone) ===\n");
    printf("Listening... Press Ctrl+C to stop\n");

    const size_t chunk_size = NDWK_VAD_WINDOW_SIZE;
    float chunk[NDWK_VAD_WINDOW_SIZE];

    while (g_mic_running) {
        size_t n = mic_reader_read(mic, chunk, chunk_size);
        if (n == 0) {
            usleep(5000);
            continue;
        }
        ndwk_feed_audio(engine, chunk, n);
    }

    printf("\nStopping microphone...\n");
    ndwk_flush(engine);
    mic_reader_stop(mic);
    mic_reader_destroy(mic);
}

/* ========================================================================== 
 * mainエントリポイント
 * ========================================================================= */
int main(int argc, char *argv[]){
#ifdef __linux__
    mallopt(M_ARENA_MAX, 1); // Linux でのメモリ断片化対策
    mallopt(M_MMAP_THRESHOLD, 32768); // mmap を使う閾値を 32KB に設定
#endif

    const char *wav_path = NULL;
    bool auto_detect = false;
    bool use_mic = false;
    const char *models_dir = "models";
    ndwk_lang_t lang = NDWK_LANG_JA;
    bool enable_punct = true;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--mic") == 0) {
            use_mic = true;
        } else if (strcmp(argv[i], "--no-punct") == 0) {
            enable_punct = false;
        } else if (strcmp(argv[i], "auto") == 0) {
            auto_detect = true;
        } else if (argv[i][0] != '-') {
            if (strstr(argv[i], ".wav") != NULL) {
                wav_path = argv[i];
            } else {
                auto_detect = false;
                lang = lang_from_string(argv[i]);
            }
        }
    }

    if (!use_mic && !wav_path) {
        wav_path = "test/ja/ja_014.wav";
    }

    printf("=== ndwk Speech Recognition ===\n");
    printf("Input: %s\n", use_mic ? "Live Microphone" : wav_path);
    printf("Mode:  %s\n\n", auto_detect ? "Auto Language Detection" : lang_to_string(lang));
    printf("Punctuation: %s\n\n", enable_punct ? "Enabled" : "Disabled");
    printf("Models: %s\n\n", models_dir);

    // ndwk 設定構造体を初期化
    ndwk_config_t cfg = ndwk_default_config();
    cfg.models_dir = models_dir;
    cfg.default_lang = lang;
    cfg.auto_detect = auto_detect;
    cfg.enable_punct = enable_punct;
    cfg.on_partial = cli_on_partial;
    cfg.on_final = cli_on_final;

    ndwk_t *engine = ndwk_create(&cfg);
    if (!engine) {
        fprintf(stderr, "Error: Failed to initialize ndwk engine\n");
        return 1;
    }

    if (use_mic) {
        run_mic(engine);
    } else {
        run_wav(engine, wav_path);
    }

    ndwk_destroy(engine);
    printf("Done.\n");
    return 0;
}
