#include "model_config.h"
#include <stdio.h>
#include <string.h>

SherpaOnnxOfflineRecognizerConfig model_config_create_asr(
    const char *models_dir,
    ndwk_lang_t lang
) {
    SherpaOnnxOfflineRecognizerConfig config;
    memset(&config, 0, sizeof(config));

    static char encoder_path[512];
    static char decoder_path[512];
    static char joiner_path[512];
    static char tokens_path[512];

    switch (lang) {
        case NDWK_LANG_JA: {
        const char *sub = "sherpa-onnx-zipformer-ja-en-reazonspeech-2025-01-17";
        snprintf(encoder_path, sizeof(encoder_path), "%s/%s/encoder-epoch-35-avg-1.int8.onnx", models_dir, sub);
        snprintf(decoder_path, sizeof(decoder_path), "%s/%s/decoder-epoch-35-avg-1.int8.onnx", models_dir, sub);
        snprintf(joiner_path,  sizeof(joiner_path),  "%s/%s/joiner-epoch-35-avg-1.int8.onnx", models_dir, sub);
        snprintf(tokens_path,  sizeof(tokens_path),  "%s/%s/tokens.txt", models_dir, sub);
        config.model_config.transducer.encoder = encoder_path;
        config.model_config.transducer.decoder = decoder_path;
        config.model_config.transducer.joiner  = joiner_path;
        config.model_config.tokens             = tokens_path;
        config.model_config.num_threads        = 4;
        config.model_config.model_type         = "transducer"; // 警告消去用
        config.model_config.modeling_unit      = "cjkchar";
        config.decoding_method                 = "modified_beam_search";
        config.feat_config.sample_rate         = 16000;
        config.feat_config.feature_dim         = 80;
        break;
    }
    default:
        fprintf(stderr, "[model_config] Unsupported lang: %d\n", lang);
        break;
    }
    return config;
}

SherpaOnnxVadModelConfig model_config_create_vad(const char *models_dir) {
    SherpaOnnxVadModelConfig config;
    memset(&config, 0, sizeof(config));

    static char vad_model_path[512];
    snprintf(vad_model_path, sizeof(vad_model_path), "%s/silero_vad.onnx", models_dir);

    config.silero_vad.model = vad_model_path;
    config.silero_vad.threshold = 0.5f;
    config.silero_vad.min_silence_duration = 0.35f; // 0.35s 無音で終端判定
    config.silero_vad.min_speech_duration = 0.25f;  // 0.25s 以上の発話
    config.silero_vad.window_size = 512;
    config.sample_rate = 16000;
    config.num_threads = 1;

    return config;
}
