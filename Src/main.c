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
    ndwk_lang_t lang = NDWK_LANG_JA;

    // 引数チェック: ./build/ndwk --mic [言語] または ./build/ndwk <WAVパス> [言語]
    if (argc >= 2) {
        if (strcmp(argv[1], "--mic") == 0) {
            use_mic = true;
        } else {
            wav_path = argv[1];
        }
    }
    if (argc >= 3 && strcmp(argv[2], "auto") != 0) {
        auto_detect = false;
        lang = lang_from_string(argv[2]);
    }

    // 引数なしならマイクをデフォルトにする、またはヘルプ
    if (!use_mic && !wav_path) {
        wav_path = "test/ja/ja_014.wav"; // デフォルト
    }

    printf("=== ndwk Speech Recognition ===\n");
    printf("Input: %s\n", use_mic ? "Live Microphone" : wav_path);
    printf("Mode:  %s\n\n", auto_detect ? "Auto Language Detection" : lang_to_string(lang));

    pipeline_t *pipeline = pipeline_create(models_dir, lang, auto_detect);
    if (!pipeline) return 1;

    if (use_mic) {
        // マイクモード起動
        pipeline_run_mic(pipeline);
    } else {
        // WAVモード起動
        wav_data_t wav;
        if (wav_reader_read(wav_path, &wav) == 0) {
            pipeline_run_wav(pipeline, &wav);
            wav_reader_free(&wav);
        }
    }

    pipeline_destroy(pipeline);
    printf("Done.\n");
    return 0;
}
