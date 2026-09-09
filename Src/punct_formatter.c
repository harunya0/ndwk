/**
 * @file punct_formatter.c
 * @brief 日本語句読点フォーマッタ (ルールベース文法補正 ＆ テキスト合成)
 *
 * 【責務】
 * 1. BERTモデルが出力したロジット (予測スコア) を解析
 * 2. 数学的等価変換により、重い超越関数 expf (シグモイド) を一切使わずに句読点を判定
 * 3. 日本語特有の文法ルールを適用：
 *    - 3文字以下の短い相槌 (「あと」「はい」等) は「。」を打たずに素通し
 *    - 接続助詞 (「ば」「て」「で」) の直後は「。」を禁止し、文を繋ぐ「、」に補正
 *    - 文末が「〜ですか」「〜ますか」等の疑問語尾なら自動的に「？」に置換
 * 4. 元の文字配列の間に「、」「。」「？」を挟み込んで最終文字列を出力バッファへ合成
 *
 * ※ ONNX Runtime や語彙テーブルについては一切知らず、
 *    純粋に「文字スパン配列 ＋ floatロジット配列 → 整形済み日本語」の変換を担当します。
 */

#include "punct_formatter.h"
#include <string.h>
#include <stdbool.h>

/* =========================================================================
 * 疑問文判定ルール
 * ========================================================================= */

/**
 * @brief 疑問符「？」を付与する文末語尾パターンの定義リスト
 *
 * 【なぜこのルールが必要なのか？】
 * - mojicast の BERT モデルは「読点 (、)」と「句点 (。)」の2クラス分類しか学習していません。
 * - モデル自体は「？」を予測できないため、文末が「〜ですか」「〜ますか」などの
 *   明らかな疑問語尾で終わっている場合に、後処理で「。」を「？」にすり替えます。
 */
static const char *g_question_suffixes[] = {
    "ですか", "ますか", "でしょうか", "かな", "かしら", "かい", "の",
    "だろうか", "でしたか", "ましたか", NULL
};

/**
 * @brief 文字列が指定したサフィックス (語尾) で終わっているかを後方比較する
 */
static bool ends_with(const char *str, size_t str_len, const char *suffix) {
    size_t suf_len = strlen(suffix);
    if (str_len < suf_len) return false;
    return memcmp(str + (str_len - suf_len), suffix, suf_len) == 0;
}

/**
 * @brief 直前の1文が疑問文かどうかを判定する
 */
static bool is_question(const char *sentence, size_t sentence_len) {
    for (int i = 0; g_question_suffixes[i] != NULL; i++) {
        if (ends_with(sentence, sentence_len, g_question_suffixes[i])) return true;
    }
    return false;
}

/* =========================================================================
 * 公開関数
 * ========================================================================= */

/**
 * @brief ロジットと文字配列から、句読点付き日本語テキストを合成する
 *
 * 【モデルの出力構造 (logits 配列)】
 * - 形状: [1, seq_len, 2]
 * - トークン 0 は [CLS] なので無視。
 * - トークン i + 1 が、chars[i] の直後の位置に対応します。
 *   - logits[(token_idx << 1) + 0]: 読点 (、) のロジット
 *   - logits[(token_idx << 1) + 1]: 句点 (。) のロジット
 *
 * 【数学的最適化: expf の完全撲滅】
 * 通常のシグモイド確率: prob = 1.0 / (1.0 + exp(-logit))
 * 「確率が 50% (0.5) 以上」の条件を展開すると：
 *   1 / (1 + exp(-logit)) >= 0.5
 *   <=> 1 + exp(-logit) <= 2
 *   <=> exp(-logit) <= 1
 *   <=> -logit <= 0
 *   <=> logit >= 0.0f
 * と数学的に 100% 同値になります！
 * したがって、重い指数関数 expf を呼ぶ必要は一切なく、単に 0.0f 以上かどうかを比較するだけで済みます。
 */
void punct_format_text(
    const char_span_t *chars,
    size_t num_chars,
    const float *logits,
    char *out,
    size_t max_out_len
) {
    // ---------------------------------------------------------------------
    // ガード①: 3文字以下の超短文 (「あと」「はい」「うん」等)
    // ---------------------------------------------------------------------
    // ストリーミング音声認識では、発話の途切れで短い単語が単独で上がってくることがあります。
    // ここに「。」を打つと不自然なので、3文字以下は句読点判定を行わずそのまま返します。
    if (num_chars <= 3) {
        size_t p = 0;
        for (size_t i = 0; i < num_chars; i++) {
            memcpy(out + p, chars[i].ptr, chars[i].len);
            p += chars[i].len;
        }
        out[p] = '\0';
        return;
    }

    size_t out_pos = 0;
    size_t max_out = max_out_len - 8; // 句読点(3バイト)＋終端文字の安全マージン
    size_t sent_start = 0;           // 現在の文の開始インデックス (疑問文判定用)

    // 各文字を走査しながら、文字の後ろに記号を挟み込んでいく
    for (size_t i = 0; i < num_chars && out_pos < max_out; i++) {
        // 1. 本文の文字を出力バッファへコピー
        memcpy(out + out_pos, chars[i].ptr, chars[i].len);
        out_pos += chars[i].len;

        // 2. この文字直後のロジットを取り出す
        size_t token_idx = i + 1; // 0番目は [CLS]
        float comma_logit  = logits[(token_idx << 1) + 0]; // 読点スコア (* 2 を << 1 で代用)
        float period_logit = logits[(token_idx << 1) + 1]; // 句点スコア
        bool is_last_char  = (i == num_chars - 1);

        // -----------------------------------------------------------------
        // ルール②: 接続助詞判定 (「ば」「て」「で」)
        // -----------------------------------------------------------------
        // 「一分間で沸騰する地域もあれば」のように文が続く場合、
        // モデルの学習バイアスで「あれば。」と切ってしまうのを防ぎます。
        bool is_conj = (chars[i].len == 3 && (
            memcmp(chars[i].ptr, "ば", 3) == 0 ||
            memcmp(chars[i].ptr, "て", 3) == 0 ||
            memcmp(chars[i].ptr, "で", 3) == 0
        ));

        // -----------------------------------------------------------------
        // ルール③: 句点 (。) の判定閾値
        // -----------------------------------------------------------------
        // - 文末 (is_last_char): 発話の終わりなので比較的緩め (-1.0f 以上) で「。」を打つ。
        // - 文の途中: 途中で文を切るのは破壊的なので、確信度高め (1.0f 以上 ≒ 確率73%) を要求する。
        bool want_period = is_last_char ? (period_logit >= -1.0f) : (period_logit >= 1.0f);

        // 【判定分岐】
        // A. 句点判定: 句点条件を満たし、かつ接続助詞の直後ではなく、読点スコアより高い場合
        if (want_period && !is_conj && (period_logit >= comma_logit)) {
            out[out_pos] = '\0';
            // 直前の1文が「〜ですか」等の疑問語尾なら「？」、そうでなければ「。」
            if (is_question(out + sent_start, out_pos - sent_start)) {
                memcpy(out + out_pos, "？", 3); // UTF-8 で全角「？」は3バイト
            } else {
                memcpy(out + out_pos, "。", 3); // UTF-8 で全角「。」は3バイト
            }
            out_pos += 3;
            sent_start = out_pos; // 次の文の先頭位置を更新
        }
        // B. 読点判定: 読点スコアが出ているか、または接続助詞「ば」等で文が継続している場合
        else if ((comma_logit >= -1.5f || (period_logit >= 0.0f && is_conj)) && !is_last_char) {
            memcpy(out + out_pos, "、", 3); // UTF-8 で全角「、」は3バイト
            out_pos += 3;
        }
    }

    out[out_pos] = '\0'; // ヌル終端
}
