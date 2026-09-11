#include "punct_engine.h"
#include <string.h>
#include <stdbool.h>

struct punct_engine_t {
    char output_buffer[1024];
};

static punct_engine_t g_punct;

static const char *g_questions[] = {
    "ですか", "ますか", "でしょうか", "かな", "かい", "でしょうか", "だろうか", "のか", NULL
};

static const char *g_conjunctions[] = {
    "ば", "たら", "なら", "けど", "けれど", "ですが", "だが", "ので", "のに", "たり", "から", "ため", "れば", "まして", NULL
};

/* ========================================================================
 * 内部関数
 * ======================================================================== */

 // UTF-8 1文字のバイト数を返す
 static inline int utf8_char_len(unsigned char c) {
    if (c < 0x80) return 1;
    else if ((c & 0xE0) == 0xC0) return 2;
    else if ((c & 0xF0) == 0xE0) return 3;
    else if ((c & 0xF8) == 0xF0) return 4;
    return 1; // 不正な UTF-8 の場合は1バイトとして扱う
}

static bool ends_with(const char *str, size_t str_len, const char *suffix) {
    size_t suffix_len = strlen(suffix);
    if (str_len < suffix_len) return false;
    return strncmp(str + (str_len - suffix_len), suffix, suffix_len) == 0;
}

/*
 *公開関数
 */
punct_engine_t *punct_engine_create(void) {
    punct_engine_t *p = &g_punct;
    memset(p, 0, sizeof(*p));
    return p;
}

const char *punct_engine_restore(punct_engine_t *engine, const char *text) {
    if (!engine || !text || text[0] == '\0') return "";

    size_t in_len = strlen(text);

    size_t num_chars = 0;
    for (size_t i = 0; i < in_len; ) {
        i += utf8_char_len((unsigned char)text[i]);
        num_chars++;
    }
    if (num_chars <= 3) {
        strncpy(engine->output_buffer, text, sizeof(engine->output_buffer) - 1);
        engine->output_buffer[sizeof(engine->output_buffer) - 1] = '\0';
        return engine->output_buffer;
    }

    size_t out_pos = 0;
    size_t max_out = sizeof(engine->output_buffer) - 8;
    size_t chars_since_punct = 0;

    for (size_t in_pos = 0; in_pos < in_len && out_pos < max_out; ) {
        int clen = utf8_char_len((unsigned char)text[in_pos]);

        memcpy(engine->output_buffer + out_pos, text + in_pos, clen);
        out_pos += clen;
        in_pos += clen;
        chars_since_punct++;

        if (in_pos < in_len && chars_since_punct >= 4) {
            for (int k = 0; g_conjunctions[k] != NULL; k++) {
                if (ends_with(engine->output_buffer, out_pos, g_conjunctions[k])) {
                    memcpy(engine->output_buffer + out_pos, "、", 3);
                    out_pos += 3;
                    chars_since_punct = 0;
                    break;
                }
            }
        }
    }

    engine->output_buffer[out_pos] = '\0';

    bool is_question = false;
    for (int k = 0; g_questions[k] != NULL; k++) {
        if (ends_with(engine->output_buffer, out_pos, g_questions[k])) {
            is_question = true;
            break;
        }
    }

    if (out_pos < max_out) {
        if (is_question) {
            memcpy(engine->output_buffer + out_pos, "？", 3);
            out_pos += 3;
        } else {
            memcpy(engine->output_buffer + out_pos, "。", 3);
            out_pos += 3;
        }
        engine->output_buffer[out_pos] = '\0';
    }

    engine->output_buffer[out_pos] = '\0';
    return engine->output_buffer;
}
