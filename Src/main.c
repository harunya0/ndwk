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
#include "ndwk_types.h"
#include "wav_reader.h"
#include "pipeline.h"
#include "lang_detector.h"
#include "punct_engine.h"

int main(int argc, char *argv[]) {
#ifdef __linux__
    mallopt(M_ARENA_MAX, 1);
    mallopt(M_MMAP_THRESHOLD, 32768);
#endif

    const char *wav_path = NULL;
    bool auto_detect = true;
    bool use_mic = false;
    const char *models_dir = "models";
    ndwk_lang_t lang = NDWK_LANG_JA; // デフォルト言語: 日本語

   bool enable_punct = true; // 句読点自動挿入を有効化
   
   for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--mic") == 0) {
            use_mic = true;
        } else if (strcmp(argv[i], "--no-punct") == 0) {
            enable_punct = false;
        } else if (strcmp(argv[i], "auto") == 0) {
            auto_detect = true;
        } else if (argv[i][0] != '-') {
            if (strstr(argv[i], ".wav") && strstr(argv[i], "/")) {
                wav_path = argv[i];
            } else {
                auto_detect = false;
                lang = lang_from_string(argv[i]);
            }
        }
    }

    // 引数が指定されなかった場合のデフォルト動作
    if (!use_mic && !wav_path) {
        wav_path = "test/ja/ja_014.wav";
    }

    printf("=== ndwk Speech Recognition ===\n");
    printf("Input: %s\n", use_mic ? "Live Microphone" : wav_path);
    printf("Mode:  %s\n\n", auto_detect ? "Auto Language Detection" : lang_to_string(lang));
    printf("Punctuation: %s\n\n", enable_punct ? "Enabled" : "Disabled");
    printf("Models: %s\n\n", models_dir);

    // パイプラインインスタンスの生成
    pipeline_t *pipeline = pipeline_create(models_dir, lang, auto_detect, enable_punct);
    if (!pipeline) {
        fprintf(stderr, "Error: Failed to initialize pipeline.\n");
        return 1;
    }

    if (use_mic) {
        // マイクリアルタイムモード
        pipeline_run_mic(pipeline);
    } else {
        // WAV ファイルストリーミングモード
        wav_data_t wav;
        if (wav_reader_read(wav_path, &wav) == 0) {
            pipeline_run_wav(pipeline, &wav);
            wav_reader_free(&wav);
        } else {
            fprintf(stderr, "Error: Could not read WAV file: %s\n", wav_path);
        }
    }

    pipeline_destroy(pipeline);
    printf("Done.\n");
    return 0;
}
