#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "ndwk_types.h"
#include "wav_reader.h"
#include "model_config.h"
#include "vad_detector.h"
#include "asr_engine.h"
#include "audio_history.h"
#include "lang_detector.h"

int main(int argc, char *argv[]) {
    const char *wav_path = "test/ja/ja_014.wav";
    const char *models_dir = "models";
    const char *lang_arg = "auto"; // デフォルトは自動検知！

    // コマンドライン引数の受け取り
    // 使い方 1: ./build/ndwk <WAVパス>         (自動判定)
    // 使い方 2: ./build/ndwk <WAVパス> <言語>  (手動指定: ja, en, zh, ko)
    if (argc >= 2) wav_path = argv[1];
    if (argc >= 3) lang_arg = argv[2];

    bool is_auto = (strcmp(lang_arg, "auto") == 0);

    printf("=== ndwk Multilingual Speech Recognition ===\n");
    printf("Target WAV: %s\n", wav_path);

    // 1. WAV読み込み
    wav_data_t wav;
    if (wav_reader_read(wav_path, &wav) != 0) {
        fprintf(stderr, "Failed to read WAV file: %s\n", wav_path);
        return 1;
    }

    // 2. モードに応じた初期化
    lang_detector_t *lid = NULL;
    asr_engine_t *asr = NULL;

    if (is_auto) {
        printf("Mode: [Auto Language Detection] (whisper-tiny LID)\n\n");
        lid = lang_detector_create(models_dir);
    } else {
        ndwk_lang_t lang = lang_from_string(lang_arg);
        printf("Mode: [Manual Lang = %s] (LID disabled, Memory-saving)\n\n", lang_to_string(lang));
        asr = asr_engine_create(models_dir, lang);
    }

    vad_detector_t *vad = vad_detector_create(models_dir);
    audio_history_t *history = audio_history_create(16000, 10.0f);

    if ((!is_auto && !asr) || (is_auto && !lid) || !vad || !history) {
        fprintf(stderr, "Initialization failed.\n");
        return 1;
    }

    // 3. VAD に音声を流し込む
    audio_history_push(history, wav.samples, wav.num_samples);
    vad_detector_accept(vad, wav.samples, wav.num_samples);
    vad_detector_flush(vad);

    // 4. セグメントの処理ループ
    vad_segment_t seg;
    int seg_count = 0;
    ndwk_lang_t current_lang = NDWK_LANG_COUNT; // キャッシュ用

    while (vad_detector_pop_segment(vad, &seg)) {
        seg_count++;
        printf("--- [Segment %d] (%.2f 秒) ---\n",
               seg_count, (double)seg.num_samples / 16000.0);

        // プリロール結合
        size_t full_samples = 0;
        float *full_audio = audio_history_with_preroll(
                history, seg.start_sample, seg.samples, seg.num_samples, &full_samples);

        // 自動検知モードの場合、言語を判定して必要な ASR を用意する
        if (is_auto) {
            ndwk_lang_t detected = lang_detector_detect(lid, full_audio, full_samples);
            printf("[Detected: %s]\n", lang_to_string(detected));

            // 前のセグメントと違う言語ならモデルを作り直す (省メモリ化)
            if (detected != current_lang) {
                if (asr) asr_engine_destroy(asr);
                asr = asr_engine_create(models_dir, detected);
                current_lang = detected;
            }
        }

        // デコード実行
        if (asr) {
            const char *text = asr_engine_transcribe(asr, full_audio, full_samples);
            printf("Transcription: %s\n\n", text);
        }

        free(full_audio);
    }

    if (seg_count == 0) {
        printf("No speech segments detected.\n");
    }

    // 5. 後始末
    printf("Cleaning up...\n");
    if (asr) asr_engine_destroy(asr);
    if (lid) lang_detector_destroy(lid);
    audio_history_destroy(history);
    vad_detector_destroy(vad);
    wav_reader_free(&wav);

    printf("Done.\n");
    return 0;
}
