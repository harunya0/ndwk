/**
 * @file punct_tokenizer.h
 * @brief 日本語 BERT 句読点モデル用トークナイザ ＆ オープンアドレス法ハッシュ語彙表
 *
 * 【責務】
 * - 語彙ファイル (`vocab.txt`) を 2の冪乗サイズ (16384スロット) の静的ハッシュ表にロード
 * - UTF-8 境界判定マスク (`0xC0 != 0x80`) による 1 文字単位の UTF-8 文字列分割
 * - テキストから BERT 入力用トークン ID (`input_ids`) および注目マスク (`attention_mask`) の生成
 * - `[CLS]` (文頭), `[SEP]` (文末) 特殊トークンの自動付与
 */

#ifndef NDWK_PUNCT_TOKENIZER_H
#define NDWK_PUNCT_TOKENIZER_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

/* =========================================================================
 * 定数定義 (2の冪乗ハッシュテーブル)
 * ========================================================================= */

/**
 * @brief ハッシュ表容量: 2^14 = 16384 スロット
 * - mojicast-punct の語彙数は約 7,900 語であるため、負荷率は約 48% となり
 *   線形探索 (オープンアドレス法) の衝突が極めて少なく高速な $O(1)$ 検索を実現。
 */
#define VOCAB_HASH_SHIFT    14
#define VOCAB_HASH_CAPACITY (1 << VOCAB_HASH_SHIFT)
#define VOCAB_HASH_MASK     (VOCAB_HASH_CAPACITY - 1) // 0x3FFF

/**
 * @brief 語彙エントリの最大文字長 (バイト数)
 */
#define MAX_TOKEN_LEN       16

/* =========================================================================
 * 構造体定義
 * ========================================================================= */

/**
 * @struct vocab_entry_t
 * @brief ハッシュテーブルの各スロットエントリ
 */
typedef struct {
    char key[MAX_TOKEN_LEN]; /**< トークン文字列 (例: "あ", "私", "[CLS]") */
    int id;                  /**< BERT 語彙 ID (0 〜 語彙数) */
    bool used;               /**< スロット使用中フラグ */
} vocab_entry_t;

/**
 * @struct punct_vocab_t
 * @brief 静的語彙ハッシュテーブルおよび特殊トークン ID
 */
typedef struct {
    vocab_entry_t entries[VOCAB_HASH_CAPACITY]; /**< 静的ハッシュエントリ配列 */
    int unk_id;                                  /**< 未知語トークン [UNK] の ID */
    int cls_id;                                  /**< 文頭トークン [CLS] の ID */
    int sep_id;                                  /**< 文末トークン [SEP] の ID */
    int pad_id;                                  /**< パディングトークン [PAD] の ID */
} punct_vocab_t;

/**
 * @struct char_span_t
 * @brief UTF-8 1文字のポインタとバイト長を指すスパン参照
 */
typedef struct {
    const char *ptr; /**< 元テキスト中の文字先頭ポインタ */
    int len;         /**< UTF-8 バイト長 (1 〜 4 バイト) */
} char_span_t;

/* =========================================================================
 * 公開関数
 * ========================================================================= */

/**
 * @brief vocab.txt を解析し、静的ハッシュテーブルに読み込む
 *
 * @param v 語彙ハッシュテーブル構造体ポインタ
 * @param filepath vocab.txt のファイルパス
 * @return bool 読み込み成功時は true、失敗時は false
 */
bool punct_vocab_load(punct_vocab_t *v, const char *filepath);

/**
 * @brief 平文テキストを UTF-8 文字単位に分割し、BERT トークン ID 列に変換する
 *
 * @param vocab 初期化済みの語彙テーブルポインタ
 * @param text トークナイズ対象の UTF-8 平文
 * @param out_chars 分割された UTF-8 文字スパンの出力配列
 * @param out_ids BERT 入力テンソル用のトークン ID 配列 ([CLS] ... [SEP])
 * @param out_mask BERT 入力テンソル用の Attention Mask 配列 (すべて 1)
 * @param max_seq_len 出力可能な最大系列長 (例: 256)
 * @return size_t 抽出された実文字数 (out_chars の要素数。[CLS], [SEP] を除く)
 */
size_t punct_tokenize(
    const punct_vocab_t *vocab,
    const char *text,
    char_span_t *out_chars,
    int64_t *out_ids,
    int64_t *out_mask,
    size_t max_seq_len
);

#endif // NDWK_PUNCT_TOKENIZER_H
