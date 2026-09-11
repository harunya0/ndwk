/**
 * @file config.h
 * @brief ndwk 音声認識パイプライン全体の設定定数・チューニングパラメータ・マクロ定義
 *
 * 【設計方針】
 * - ゼロ動的メモリ確保 (zero malloc) を実現するための静的バッファサイズ定義
 * - ビット演算 (シフト・マスク) による高速化定数の集約
 * - 組み込み (Cortex-A / マイコン) から高性能デスクトップまで容易にスケールできる設定設計
 */

#ifndef NDWK_CONFIG_H
#define NDWK_CONFIG_H

#include <stdint.h>

/* ========================================================================== */
/*  1. ターゲット環境・プロファイル設定 (Low-RAM vs 通常)                    */
/* ========================================================================== */

#ifdef NDWK_PROFILE_LOW_RAM
// --- 【マイコン・低RAMプロファイル】 (バッファ消費: 約576KB) ---
#define NDWK_NUM_THREADS                1       // 1スレッドでORTの作業メモリ削減
#define NDWK_PARTIAL_WINDOW_SEC         2.5f    // 速報窓を2.5秒にしてCPU負荷激減
#define NDWK_PARTIAL_WINDOW_SAMPLES     40000   // 16000 * 2.5秒
#define AUDIO_HISTORY_SHIFT             17      // 2^17 = 131,072 サンプル (約8.2秒分, 計1MB)
#define MIC_RB_SHIFT                    14      // 2^14 = 16,384 サンプル (約1.0秒分, 計64KB)

#else
// --- 【通常デスクトッププロファイル】 (バッファ消費: 約2.1MB) ---
#define NDWK_NUM_THREADS                2
#define NDWK_PARTIAL_WINDOW_SEC         3.0f
#define NDWK_PARTIAL_WINDOW_SAMPLES     48000   // 16000 * 3.0秒
#define AUDIO_HISTORY_SHIFT             18      // 2^18 = 262,144 サンプル (約16.4秒分, 計2MB)
#define MIC_RB_SHIFT                    15      // 2^15 = 32,768 サンプル (約2.0秒分, 計128KB)
#endif

// 2の冪乗サイズとビットマスクの共通計算
#define AUDIO_HISTORY_CAPACITY          (1 << AUDIO_HISTORY_SHIFT)
#define AUDIO_HISTORY_MASK              (AUDIO_HISTORY_CAPACITY - 1)

#define MIC_RB_CAPACITY                 (1 << MIC_RB_SHIFT)
#define MIC_RB_MASK                     (MIC_RB_CAPACITY - 1)


/**
 * @brief 速報字幕 (Partial) の推論・画面更新間隔 (秒)
 * - 推奨値: 0.30f 〜 0.50f (人間の知覚に自然で、かつ無駄なCPU消費を抑える黄金比)
 * - マイコン等でCPU負荷をさらに落としたい場合は 0.50f に延長可能
 */
#define NDWK_PARTIAL_INTERVAL_SEC   0.40f

/**
 * @brief 音声履歴リングバッファの保持秒数 (秒)
 * - メモリ消費目安: 16kHz float32 (4バイト) で 1秒あたり約 64 KB
 * - 10.0f: 約 640 KB (標準。一般的な発話区間を余裕を持ってカバー)
 * - RAMが極めてタイトな組み込み環境では 5.0f (約 320 KB) に短縮可能
 */
#define NDWK_HISTORY_KEEP_SEC       10.0f


/* ========================================================================== */
/*  2. 認識精度・VAD挙動のチューニング項目                                   */
/* ========================================================================== */

/**
 * @brief 冒頭の音声欠落を防ぐプリロール (前置音声) 秒数 (秒)
 * - 標準推奨値: 1.0f (CER: 文字誤り率を劇的に改善する重要パラメータ)
 * - VADが「発話」と判定する直前の環境音・子音の立ち上がりを巻き戻してASRに渡すことで、
 *   例えば「おはようございます」の「お」が欠落する現象を完全に防止します。
 */
#define NDWK_PREROLL_SEC            0.5f

/**
 * @brief 発話終了 (確定) とみなす無音継続時間 (秒)
 * - 標準値: 0.5f
 * - 小さくする (例: 0.25f): 発話終了から確定字幕が出るまでの体感速度が最速化するが、
 *   息継ぎや短い間接詞で文が細切れに分断されやすくなる
 * - 大きくする (例: 0.60f): 文を1つにまとめやすくなるが、確定までの待ち時間が長くなる
 */
#define NDWK_VAD_MIN_SILENCE_SEC    0.5f

/**
 * @brief 発話中でも一定間隔ごとに強制分割する最大発話継続時間 (秒)
 * - 標準値: 8.0f (長時間の独白や講演でも、途中でASRを区切って確定字幕を出すことで、
 *   体感速度を改善する)
 * - 小さくする (例: 5.0f): 長い発話でも途中で確定字幕が出るため、体感速度はさらに改善するが、
 *   発話の自然な流れが損なわれる可能性がある。
 */
 #define NDWK_VAD_MAX_SPEECH_SEC     8.0f

/**
 * @brief VAD の発話検知スコア閾値 (0.0 〜 1.0)
 * - 標準値: 0.6f
 * - 下げる (例: 0.3f): 囁き声や小さな声も拾えるが、エアコン音やマイクノイズに誤反応しやすい
 * - 上げる (例: 0.7f): 雑音に強くなるが、ハッキリ喋らないと認識が始まらない
 */
#define NDWK_VAD_THRESHOLD          0.6f

/**
 * @brief 発話開始とみなす最小音声継続時間 (秒)
 * - 標準値: 0.3f
 * - 咳払い、マイク接触音、舌打ち等の極小インパルスノイズを無視するためのフィルタ
 */
#define NDWK_VAD_MIN_SPEECH_SEC     0.3f


/* ========================================================================== */
/*  3. 音声ハードウェア・AIモデル仕様定数 (変更厳禁)                         */
/* ========================================================================== */

/**
 * @warning 【変更厳禁】入力サンプリング周波数 (Hz)
 * 本システムで利用する全ての ONNX モデル (ReazonSpeech, Silero VAD, Whisper, Paraformer等) が
 * 16kHz 単一チャンネル (モノラル) float32 前提で学習されているため、変更不可。
 */
#define NDWK_SAMPLE_RATE            16000

/**
 * @warning 【変更厳禁】Silero VAD 処理単位 (サンプル数)
 * Silero VAD モデルの入力テンソル形状は 512 サンプル (32ms) 固定です。
 * 512 は 2^9 であり、ビット演算による高速化が可能です。
 */
#define NDWK_VAD_WINDOW_SIZE        512
#define NDWK_VAD_WINDOW_SHIFT       9                            // 512 == 1 << 9
#define NDWK_VAD_WINDOW_MASK        (NDWK_VAD_WINDOW_SIZE - 1)   // 511 (0x1FF)

/**
 * @brief サンプル数に換算された主要インターバル定数 (16kHz基準)
 * コンパイル時定数化により、実行時の浮動小数点乗算をゼロにします。
 */
#define NDWK_PARTIAL_INTERVAL_SAMPLES   6400   // 16000 * 0.40秒
#define NDWK_PREROLL_SAMPLES            8000  // 16000 * 0.5秒
#define NDWK_LID_MAX_SAMPLES            64000  // 16000 * 4.0秒 (言語判別に渡す最大サンプル数)

/**
 * @brief リアルタイム再生シミュレーション用: 1サンプルあたりの時間 (ナノ秒)
 * 1秒 = 1,000,000,000 ナノ秒
 * 1,000,000,000 / 16,000 = 62,500 ナノ秒 (割り切れる整数)
 * サンプル数 × 62500LL で即座に経過ナノ秒を算出可能 (除算不要)。
 */
#define NDWK_SAMPLE_TO_NS               62500LL


/* ========================================================================== */
/*  4. ビット演算・バイト計算用シフト定数                                     */
/* ========================================================================== */

/**
 * @brief sizeof(float) == 4 (2^2) 用のビットシフト
 * count * sizeof(float) を count << NDWK_FLOAT_SHIFT で超高速に計算
 */
#define NDWK_FLOAT_SHIFT            2

/**
 * @brief sizeof(int64_t) == 8 (2^3) 用のビットシフト
 * count * sizeof(int64_t) を count << NDWK_INT64_SHIFT で超高速に計算
 */
#define NDWK_INT64_SHIFT            3


/* ========================================================================== */
/*  5. 分岐予測最適化マクロ (Linux カーネル互換)                              */
/* ========================================================================== */

/**
 * @brief ホットパス (最頻実行コード) をストレートライン化し、
 *        CPUの分岐予測ミスによるパイプラインストールを防止するマクロ
 */
#if defined(__GNUC__) || defined(__clang__)
#define likely(x)   __builtin_expect(!!(x), 1)
#define unlikely(x) __builtin_expect(!!(x), 0)
#else
#define likely(x)   (x)
#define unlikely(x) (x)
#endif

#endif // NDWK_CONFIG_H
