/**
 * @file ndwk_types.h
 * @brief ndwk 音声認識パイプライン共通の型定義・列挙型
 */

#ifndef NDWK_TYPES_H
#define NDWK_TYPES_H

/**
 * @enum ndwk_lang_t
 * @brief 音声認識エンジンがサポートする言語識別子
 *
 * 各言語は、それぞれの言語特性に特化した最先端の ONNX 音響モデルにマッピングされます。
 */
typedef enum {
    NDWK_LANG_JA,      /**< 日本語: ReazonSpeech Zipformer (高精度・超高速 modified_beam_search) */
    NDWK_LANG_ZH,      /**< 中国語: Paraformer-zh (非自己回帰型 Non-autoregressive 高速推論) */
    NDWK_LANG_KO,      /**< 韓国語: SenseVoice Small (多言語・感情/イディオム対応) */
    NDWK_LANG_EN,      /**< 英語・欧州言語: NVIDIA NeMo Parakeet TDT v3 (Fast Conformer 0.6B) */
    NDWK_LANG_OMNI,    /**< 将来拡張用: Omnilingual 多言語モデル */
    NDWK_LANG_COUNT    /**< サポート言語の総数 */
} ndwk_lang_t;

#endif // NDWK_TYPES_H
