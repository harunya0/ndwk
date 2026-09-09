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

int main(int argc, char *argv[]) {
    const char *wav_path = NULL;
    const char *models_dir = "models";
    bool auto_detect = true;
    bool use_mic = false;
    ndwk_lang_t lang = NDWK_LANG_JA; // デフォルト言語: 日本語

    // 第1引数の解析 (--mic または WAV ファイルパス)
    if (argc >= 2) {
        if (strcmp(argv[1], "--mic") == 0) {
            use_mic = true;
        } else {
            wav_path = argv[1];
        }
    }

    // 第2引数の解析 (言語コード指定: ja, zh, en, ko, または auto)
    if (argc >= 3 && strcmp(argv[2], "auto") != 0) {
        auto_detect = false;
        lang = lang_from_string(argv[2]);
    }

    // 引数が指定されなかった場合のデフォルト動作
    if (!use_mic && !wav_path) {
        wav_path = "test/ja/ja_014.wav";
    }

    printf("=== ndwk Speech Recognition ===\n");
    printf("Input: %s\n", use_mic ? "Live Microphone" : wav_path);
    printf("Mode:  %s\n\n", auto_detect ? "Auto Language Detection" : lang_to_string(lang));

    // パイプラインインスタンスの生成
    pipeline_t *pipeline = pipeline_create(models_dir, lang, auto_detect);
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
