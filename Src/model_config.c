/**
 * @file model_config.c
 * @brief ASR / VAD モデル設定ファクトリの実装
 *
 * 【サポートモデル一覧と特徴】
 * 1. 日本語 (NDWK_LANG_JA):
 *    - ReazonSpeech Zipformer (sherpa-onnx-zipformer-ja-en-reazonspeech-2025-01-17)
 *    - 方式: Transducer (Encoder + Decoder + Joiner)
 *    - デコード: modified_beam_search, モデリング単位: cjkchar, int8 量子化
 *
 * 2. 中国語 (NDWK_LANG_ZH):
 *    - Paraformer-zh (sherpa-onnx-paraformer-zh-int8-2025-10-07)
 *    - 方式: 非自己回帰型 (Non-autoregressive), 超高速推論
 *
 * 3. 英語・欧州 (NDWK_LANG_EN):
 *    - NVIDIA NeMo Parakeet TDT v3 (sherpa-onnx-nemo-parakeet-tdt-0.6b-v3-int8)
 *    - 方式: NeMo Transducer (Fast Conformer 0.6B)
 *
 * 4. 韓国語 (NDWK_LANG_KO):
 *    - SenseVoice Small (sherpa-onnx-sense-voice-zh-en-ja-ko-yue-int8-2024-07-17)
 *    - 方式: SenseVoice (ITN: Inverse Text Normalization 逆テキスト正規化 有効)
 *
 * 5. VAD (音声区間検出):
 *    - Silero VAD (silero_vad.onnx), 入力窓 512 サンプル (32ms)
 */

#include "model_config.h"
#include "config.h"
#include <stdio.h>
#include <string.h>

SherpaOnnxOfflineRecognizerConfig model_config_create_asr(
    const ndwk_config_t *cfg, ndwk_lang_t lang) {

    SherpaOnnxOfflineRecognizerConfig config;
    memset(&config, 0, sizeof(config));

    // パス格納用静的バッファ (関数終了後も Sherpa-ONNX 内部から参照可能にするため static)
    static char model_path[512];
    static char encoder_path[512];
    static char decoder_path[512];
    static char joiner_path[512];
    static char tokens_path[512];

    const char *models_dir = cfg->models_dir;
    int32_t num_threads = cfg->num_threads;

    switch (lang) {
    case NDWK_LANG_JA: {
        // 日本語: ReazonSpeech Zipformer
        const char *sub = "sherpa-onnx-zipformer-ja-en-reazonspeech-2025-01-17";
        snprintf(encoder_path, sizeof(encoder_path), "%s/%s/encoder-epoch-35-avg-1.int8.onnx", models_dir, sub);
        snprintf(decoder_path, sizeof(decoder_path), "%s/%s/decoder-epoch-35-avg-1.int8.onnx", models_dir, sub);
        snprintf(joiner_path,  sizeof(joiner_path),  "%s/%s/joiner-epoch-35-avg-1.int8.onnx", models_dir, sub);
        snprintf(tokens_path,  sizeof(tokens_path),  "%s/%s/tokens.txt", models_dir, sub);

        config.model_config.transducer.encoder = encoder_path;
        config.model_config.transducer.decoder = decoder_path;
        config.model_config.transducer.joiner  = joiner_path;
        config.model_config.tokens             = tokens_path;
        config.model_config.num_threads        = num_threads;
        config.model_config.model_type         = "transducer";
        config.model_config.modeling_unit      = "cjkchar";
        config.decoding_method                 = "modified_beam_search";
        config.feat_config.sample_rate         = NDWK_SAMPLE_RATE;
        config.feat_config.feature_dim         = 80;
        break;
    }
    case NDWK_LANG_ZH: {
        // 中国語: Paraformer-zh
        const char *sub = "sherpa-onnx-paraformer-zh-int8-2025-10-07";
        snprintf(model_path,  sizeof(model_path),  "%s/%s/model.int8.onnx", models_dir, sub);
        snprintf(tokens_path, sizeof(tokens_path), "%s/%s/tokens.txt", models_dir, sub);

        config.model_config.paraformer.model = model_path;
        config.model_config.tokens          = tokens_path;
        config.model_config.num_threads     = num_threads;
        config.model_config.debug           = 0;
        config.feat_config.sample_rate      = NDWK_SAMPLE_RATE;
        config.feat_config.feature_dim      = 80;
        break;
    }
    case NDWK_LANG_EN: {
        // 英語・欧州: NVIDIA NeMo Parakeet TDT v3
        const char *sub = "sherpa-onnx-nemo-parakeet-tdt-0.6b-v3-int8";
        snprintf(encoder_path, sizeof(encoder_path), "%s/%s/encoder.int8.onnx", models_dir, sub);
        snprintf(decoder_path, sizeof(decoder_path), "%s/%s/decoder.int8.onnx", models_dir, sub);
        snprintf(joiner_path,  sizeof(joiner_path),  "%s/%s/joiner.int8.onnx", models_dir, sub);
        snprintf(tokens_path,  sizeof(tokens_path),  "%s/%s/tokens.txt", models_dir, sub);

        config.model_config.transducer.encoder = encoder_path;
        config.model_config.transducer.decoder = decoder_path;
        config.model_config.transducer.joiner  = joiner_path;
        config.model_config.tokens             = tokens_path;
        config.model_config.num_threads        = num_threads;
        config.model_config.model_type         = "nemo_transducer";
        config.decoding_method                 = "greedy_search";
        config.feat_config.sample_rate         = NDWK_SAMPLE_RATE;
        config.feat_config.feature_dim         = 80;
        break;
    }
    case NDWK_LANG_KO: {
        // 韓国語: SenseVoice Small
        const char *sub = "sherpa-onnx-sense-voice-zh-en-ja-ko-yue-int8-2024-07-17";
        snprintf(model_path,  sizeof(model_path),  "%s/%s/model.int8.onnx", models_dir, sub);
        snprintf(tokens_path, sizeof(tokens_path), "%s/%s/tokens.txt", models_dir, sub);

        config.model_config.sense_voice.model = model_path;
        config.model_config.sense_voice.language = "ko";
        config.model_config.sense_voice.use_itn = 1; // 逆テキスト正規化
        config.model_config.tokens          = tokens_path;
        config.model_config.num_threads     = num_threads;
        config.feat_config.sample_rate      = NDWK_SAMPLE_RATE;
        config.feat_config.feature_dim      = 80;
        break;
    }
    default:
        fprintf(stderr, "[model_config] Error: Unsupported lang: %d\n", lang);
        break;
    }

    return config;
}

SherpaOnnxVadModelConfig model_config_create_vad(const ndwk_config_t *cfg) {
    SherpaOnnxVadModelConfig config;
    memset(&config, 0, sizeof(config));

    static char vad_model_path[512];
    snprintf(vad_model_path, sizeof(vad_model_path), "%s/silero_vad.onnx", cfg->models_dir);

    config.silero_vad.model = vad_model_path;
    config.silero_vad.threshold = cfg->vad_threshold;
    config.silero_vad.min_silence_duration = cfg->vad_min_silence_sec;
    config.silero_vad.min_speech_duration = cfg->vad_min_speech_sec;
    config.silero_vad.max_speech_duration = cfg->vad_max_speech_sec;
    config.silero_vad.window_size = NDWK_VAD_WINDOW_SIZE;
    config.sample_rate = NDWK_SAMPLE_RATE;
    config.num_threads = 1; // VAD は超軽量モデルのため 1 スレッドで十分

    return config;
}
