#ifndef MODEL_CONFIG_H
#define MODEL_CONFIG_H

#include "ndwk_types.h"
#include "sherpa-onnx/c-api/c-api.h"

// 指定した言語用の ASR 設定を構築する
SherpaOnnxOfflineRecognizerConfig model_config_create_asr(
    const char *models_dir, ndwk_lang_t lang);
// Silero VAD 用の設定を構築する
SherpaOnnxVadModelConfig model_config_create_vad(
    const char *models_dir);

#endif // MODEL_CONFIG_H
