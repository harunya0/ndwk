#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ndwk_types.h"
#include "wav_reader.h"
#include "pipeline.h"
#include "lang_detector.h"

int main(int argc, char *argv[]) {
    const char *wav_path = "test/ja/ja_014.wav";
    const char *models_dir = "models";
    bool auto_detect = true;
    ndwk_lang_t lang = NDWK_LANG_JA;

    if (argc >= 2) wav_path = argv[1];
    if (argc >= 3 && strcmp(argv[2], "auto") != 0) {
        auto_detect = false;
        lang = lang_from_string(argv[2]);
    }

    printf("=== ndwk Speech Recognition ===\n");
    printf("WAV:  %s\n", wav_path);
    printf("Mode: %s\n\n", auto_detect ? "Auto Language Detection" : lang_to_string(lang));

    // 1. WAV読み込み
    wav_data_t wav;
    if (wav_reader_read(wav_path, &wav) != 0) {
        fprintf(stderr, "Failed to read WAV file: %s\n", wav_path);
        return 1;
    }

    // 2. パイプラインを作成して実行
    pipeline_t *pipeline = pipeline_create(models_dir, lang, auto_detect);
    if (pipeline) {
        pipeline_run_wav(pipeline, &wav);
        pipeline_destroy(pipeline);
    }

    // 3. 後始末
    wav_reader_free(&wav);
    printf("\nDone.\n");
    return 0;
}
