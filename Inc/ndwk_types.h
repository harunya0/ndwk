#ifndef NDWK_TYPES_H
#define NDWK_TYPES_H

typedef enum {
    NDWK_LANG_JA,      // 日本語: ReazonSpeech Zipformer
    NDWK_LANG_ZH,      // 中国語: Paraformer-zh
    NDWK_LANG_KO,      // 韓国語: SenseVoice
    NDWK_LANG_EN,      // 英語+欧州: Parakeet TDT v3
    NDWK_LANG_OMNI,    // その他: Omnilingual ASR
    NDWK_LANG_COUNT
} ndwk_lang_t;

#endif // NDWK_TYPES_H