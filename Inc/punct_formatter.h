/**
 * @file punct_formatter.h
 * @brief 日本語句読点フォーマッタ (ルールベース文法補正 ＆ テキスト合成)
 *
 * 【責務】
 * - BERT モデルが出力したロジット (予測スコア) を解析
 * - 数学的等価判定 (logit >= 0.0f) により、重い超越関数 expf (シグモイド) を完全排除
 * - 短発話保護、接続助詞「ば・て・で」の句点禁止、疑問語尾「〜ですか」等の文法ルールを適用
 * - 元の文字配列の間に「、」「。」「？」を挟み込んで最終日本語文字列を出力
 */

#ifndef NDWK_PUNCT_FORMATTER_H
#define NDWK_PUNCT_FORMATTER_H

#include "punct_tokenizer.h"

/**
 * @brief BERT の予測ロジットと文字スパン配列から、句読点付きの日本語文を合成出力する
 *
 * @param chars トークナイズで得られた UTF-8 文字スパン配列
 * @param num_chars 文字数
 * @param logits BERT モデルから出力された予測スコア配列 (形状: [seq_len, 3])
 * @param out 整形済み文字列の書き込み先バッファ
 * @param max_out_len 出力バッファの最大容量 (バイト数)
 */
void punct_format_text(
    const char_span_t *chars,
    size_t num_chars,
    const float *logits,
    char *out,
    size_t max_out_len
);

#endif // NDWK_PUNCT_FORMATTER_H
