/**
 * @file ndwk_types.h
 * @brief ndwk 音声認識パイプライン共通の型定義・列挙型
 */

#ifndef NDWK_TYPES_H
#define NDWK_TYPES_H

#include <stdint.h>

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

/**
 * @struct ndwk_frame_meta_t
 * @brief 1フレーム（32ms = 512サンプル）ごとの音響・認識メタデータ構造体
 */
typedef struct {
    uint64_t frame_index;        /**< 音声投入開始からの累積フレーム番号 (0起算) */
    float    f0;                 /**< 基本周波数/ピッチ [Hz] (0.0f は無声音・無音) */
    float    delta_f0;           /**< 直近有声フレームとのF0差分 [Hz] (ピッチの傾き) */
    float    rms;                /**< 短時間RMS (二乗平均平方根振幅) */
    float    energy;             /**< 短時間フレームエネルギー (∑ x^2) */
    uint8_t  is_speech;          /**< VAD発話フラグ (1: 発声中, 0: 無音) */
    uint8_t  _reserved[3];       /**< 4バイトアライメント調整用パディング */
    uint32_t silence_frames;     /**< 連続無音フレーム数 */
    float    vad_prob;           /**< VAD発声スコア/確率 (0.0f〜1.0f) */
    int32_t  token_id;           /**< 直近確定トークンID (-1: トークンなし) */
    float    token_confidence;   /**< トークン確信度 (0.0f〜1.0f) */
} ndwk_frame_meta_t;

#endif // NDWK_TYPES_H
