#include <stdio.h>
#include <stdlib.h>
#include "ndwk_types.h"
#include "wav_reader.h"
#include "model_config.h"
#include "vad_detector.h"
#include "asr_engine.h"

int main(void){
    const char *wav_path = "test/ja/ja_014.wav";
    const char *models_dir = "models";

    printf("=== ndwk Japanese Speech Recognition ===\n\n");

    wav_data_t wav;
    if (wav_reader_read(wav_path, &wav) != 0) {
        fprintf(stderr, "Failed to read WAV file: %s\n", wav_path);
        return 1;
    }
    printf("1. Loaded WAV: %s (%u Hz, %zu samples, 約 %.2f 秒)\n",
           wav_path, wav.sample_rate, wav.num_samples,
           (double)wav.num_samples / wav.sample_rate);

    printf("2. Initializing ASR and VAD...\n");
    asr_engine_t *asr = asr_engine_create(models_dir, NDWK_LANG_JA);
    vad_detector_t *vad = vad_detector_create(models_dir);
    if (!asr || !vad) {
        fprintf(stderr, "Failed to initialize ASR or VAD\n");
        wav_reader_free(&wav);
        return 1;
    }

    vad_detector_accept(vad, wav.samples, wav.num_samples);
    vad_detector_flush(vad);

    printf("3. Feeding audio to VAD and ASR...\n");
    vad_segment_t seg;
    int seg_count = 0;

    while (vad_detector_pop_segment(vad, &seg)) {
        seg_count++;
        printf("--- [Segment %d] (samples: %zu, 約 %.2f 秒) ---\n",
               seg_count, seg.num_samples, (double)seg.num_samples / 16000.0);
        
        const char *text = asr_engine_transcribe(asr, seg.samples, seg.num_samples);
        printf("Transcription: %s\n", text);
    }

    if (seg_count == 0) {
        printf("No speech segments detected.\n");
    }

    printf("4. Cleaning up...\n");
    asr_engine_destroy(asr);
    vad_detector_destroy(vad);
    wav_reader_free(&wav);

    printf("Done.\n");
    return 0;
}
