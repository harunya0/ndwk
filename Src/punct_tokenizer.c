/**
 * @file punct_tokenizer.c
 * @brief 日本語文字単位トークナイザ (BERT char v3 用)
 *
 * 【責務】
 * 1. vocab.txt (約7,000行) を読み込み、ビットマスク付きハッシュテーブルを構築
 * 2. UTF-8 文字列を先頭バイトのビット判定によって 1 文字ずつ分解
 * 3. 文字列を BERT が受け付けるトークンID列 ([CLS], 文字1, 文字2, ..., [SEP]) に変換
 *
 * ※ ONNX Runtime や句読点の挿入ルールについては一切知らず、
 *    純粋に「テキスト ↔ トークンID」の変換のみを担当します。
 */

#include "punct_tokenizer.h"
#include "config.h"
#include <stdio.h>
#include <string.h>

/* =========================================================================
 * ハッシュテーブル内部処理 (オープンアドレス法 / 線形探索)
 * ========================================================================= */

/**
 * @brief 文字列ハッシュ関数 (古典的 djb2 アルゴリズムのビット演算版)
 *
 * 【なぜこの計算なのか？】
 * - 初期値 5381 はハッシュの偏りを最小化する素数。
 * - `((hash << 5) + hash)` は `hash * 33` と等価ですが、CPUの乗算器を使わず
 *   ビットシフト(<< 5)と加算だけで超高速に計算できます。
 * - 最後に `& VOCAB_HASH_MASK` (0x3FFF = 16383) を掛けることで、
 *   重い剰余算 (%) を使わずに一瞬で 0〜16383 のインデックスに収めます。
 */
static inline unsigned int hash_string(const char *str) {
    unsigned long hash = 5381;
    int c;
    while ((c = (unsigned char)*str++)) {
        hash = ((hash << 5) + hash) + c; // hash * 33 + c
    }
    return (unsigned int)(hash & VOCAB_HASH_MASK);
}

/**
 * @brief ハッシュテーブルへの単語登録
 *
 * ハッシュ値の位置がすでに使われていた場合 (衝突時) は、
 * `(idx + 1) & VOCAB_HASH_MASK` で右隣のスロットへ進む (オープンアドレス法)。
 */
static void vocab_insert(punct_vocab_t *v, const char *word, int id) {
    unsigned int idx = hash_string(word);
    while (v->entries[idx].used) {
        idx = (idx + 1) & VOCAB_HASH_MASK; // 衝突したら次のスロットへ循環
    }
    strncpy(v->entries[idx].key, word, sizeof(v->entries[idx].key) - 1);
    v->entries[idx].id = id;
    v->entries[idx].used = true;
}

/**
 * @brief ハッシュテーブルからの単語検索
 * @return 見つかればトークンID、未登録の文字なら [UNK] のID
 */
static int vocab_lookup(const punct_vocab_t *v, const char *word) {
    unsigned int idx = hash_string(word);
    while (v->entries[idx].used) {
        if (strcmp(v->entries[idx].key, word) == 0) {
            return v->entries[idx].id; // 発見！
        }
        idx = (idx + 1) & VOCAB_HASH_MASK; // 次のスロットを探す
    }
    return v->unk_id; // 辞書に存在しない未知の文字は [UNK]
}

/* =========================================================================
 * 公開関数
 * ========================================================================= */

/**
 * @brief vocab.txt を読み込み、ハッシュテーブルを構築する
 *
 * ファイルの各行の「行番号 (0始まり)」がそのまま BERT のトークンIDになります。
 * 特殊トークン ([UNK], [CLS], [SEP], [PAD]) のIDもここで特定・保持します。
 */
bool punct_vocab_load(punct_vocab_t *v, const char *filepath) {
    FILE *fp = fopen(filepath, "r");
    if (!fp) return false;

    char line[64];
    int id = 0;
    while (fgets(line, sizeof(line), fp)) {
        line[strcspn(line, "\r\n")] = '\0'; // 改行文字を除去
        if (line[0] == '\0') { id++; continue; }

        vocab_insert(v, line, id);

        // BERTの特殊トークンIDを記憶
        if (strcmp(line, "[UNK]") == 0)      v->unk_id = id; // 未知語
        else if (strcmp(line, "[CLS]") == 0) v->cls_id = id; // 先頭トークン
        else if (strcmp(line, "[SEP]") == 0) v->sep_id = id; // 終端トークン
        else if (strcmp(line, "[PAD]") == 0) v->pad_id = id; // パディング

        id++;
    }
    fclose(fp);
    return true;
}

/**
 * @brief UTF-8 先頭バイトから、その文字が何バイト構成かを判定する
 *
 * 【UTF-8 の規格仕様とビットマスク】
 * - 0xxxxxxx (c & 0x80 == 0)    : 1バイト (半角英数・ASCII)
 * - 110xxxxx (c & 0xE0 == 0xC0) : 2バイト (ラテン文字・記号など)
 * - 1110xxxx (c & 0xF0 == 0xE0) : 3バイト (日本語のひらがな・カタカナ・漢字の大半)
 * - 11110xxx (c & 0xF8 == 0xF0) : 4バイト (絵文字や特殊漢字など)
 */
static inline int utf8_char_len(unsigned char c) {
    if (likely((c & 0x80) == 0))    return 1;
    if ((c & 0xE0) == 0xC0)         return 2;
    if ((c & 0xF0) == 0xE0)         return 3;
    if ((c & 0xF8) == 0xF0)         return 4;
    return 1;
}

/**
 * @brief 入力テキストを分解し、BERTモデル用のトークンID列を作成する
 *
 * 【生成される配列の構造】
 * - out_ids[0]     : [CLS] トークン (文章の開始を表す)
 * - out_ids[1..N]  : 各文字のトークンID (ハッシュ引き結果)
 * - out_ids[N+1]   : [SEP] トークン (文章の終了を表す)
 * - out_chars[0..N-1]: 各文字へのポインタとバイト長 (後で文章を復元するため)
 * - out_mask       : 有効なトークン位置をすべて 1 で埋めたアテンションマスク
 *
 * @return 分解された文字数 N
 */
size_t punct_tokenize(
    const punct_vocab_t *vocab,
    const char *text,
    char_span_t *out_chars,
    int64_t *out_ids,
    int64_t *out_mask,
    size_t max_seq_len
) {
    // 先頭は必ず [CLS]
    out_ids[0] = vocab->cls_id;
    out_mask[0] = 1;

    size_t num_chars = 0;
    const char *p = text;

    // [CLS] と [SEP] の2枠を空けておくため (max_seq_len - 2) までループ
    while (*p != '\0' && num_chars < (max_seq_len - 2)) {
        int clen = utf8_char_len((unsigned char)*p);

        // 後で元の文字を取り出せるようにポインタと長さを保存
        out_chars[num_chars].ptr = p;
        out_chars[num_chars].len = clen;

        // 1文字だけを取り出してヌル終端文字列にする
        char token_str[MAX_TOKEN_LEN] = {0};
        int copy_len = clen < (MAX_TOKEN_LEN - 1) ? clen : (MAX_TOKEN_LEN - 1);
        memcpy(token_str, p, copy_len);

        // 辞書を引いてトークンIDに変換 (インデックスは +1 ずれる)
        out_ids[num_chars + 1] = vocab_lookup(vocab, token_str);
        out_mask[num_chars + 1] = 1;

        num_chars++;
        p += clen; // 次の文字へ進む
    }

    // 末尾に [SEP] を配置
    if (num_chars > 0) {
        size_t seq_len = num_chars + 2;
        out_ids[seq_len - 1] = vocab->sep_id;
        out_mask[seq_len - 1] = 1;
    }
    return num_chars;
}
