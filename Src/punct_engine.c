/**
 * @file punct_engine.c
 * @brief 日本語句読点復元エンジン (ONNX Runtime セッション管理 ＆ オーケストレーター)
 *
 * 【責務】
 * 1. ONNX Runtime API の初期化、セッション構築 (punct_bert.int8.onnx)
 * 2. CPU MemoryInfo の生成と保持 (推論ごとのアロケーション防止)
 * 3. 以下の3ステップを統括して句読点復元を実行：
 *    ① トークナイザ (punct_tokenizer.c) を呼び出して input_ids を作成
 *    ② ONNX Runtime の Run() を叩いて各文字のロジット配列を取得
 *    ③ フォーマッタ (punct_formatter.c) を呼び出して最終的な日本語文字列を合成
 *
 * 【メモリ設計】
 * - エンジン構造体は BSSセクション (static) に配置 (malloc = 0)
 * - 推論テンソルのバッファはスタック配列 (256要素) を使用 (malloc = 0)
 * - 出力文字列はエンジン内部の静的バッファ output_buffer に格納して返却
 */

#include "punct_engine.h"
#include "punct_tokenizer.h"
#include "punct_formatter.h"
#include "config.h"
#include "onnxruntime_c_api.h"
#include <stdio.h>
#include <string.h>

#define MAX_OUTPUT_TEXT   4096 // 復元後テキストの最大バイト数
#define MAX_PUNCT_SEQ_LEN 256  // 1回で処理する最大トークン数 (BERTの入力上限は512)

// ONNX Runtime の戻り値 OrtStatus* を安全に受け取って解放する警告消しマクロ
#define ORT_IGNORE(expr) do { OrtStatus *st_ = (expr); if (st_) engine->ort->ReleaseStatus(st_); } while (0)

struct punct_engine_t {
    const OrtApi *ort;
    OrtEnv *env;
    OrtSession *session;
    OrtMemoryInfo *mem_info;
    punct_vocab_t vocab;
    char output_buffer[MAX_OUTPUT_TEXT]; // 結果文字列用の静的バッファ
};

// BSSセクションに1個だけ静的実体を配置 (malloc完全ゼロ！)
static punct_engine_t g_punct_engine;

/**
 * @brief 句読点復元エンジンの初期化
 *
 * 1. vocab.txt をロードしてハッシュテーブルを構築
 * 2. ONNX Runtime 環境 (Env) を作成
 * 3. punct_bert.int8.onnx (109MB軽量版) を探してロード (無ければ fp32 版にフォールバック)
 * 4. 推論用テンソルで使い回す CpuMemoryInfo を事前作成・キャッシュ
 */
punct_engine_t *punct_engine_create(const char *models_dir) {
    punct_engine_t *engine = &g_punct_engine;
    memset(engine, 0, sizeof(*engine));

    // 1. vocab.txt のロード (トークナイザ層の関数を呼び出し)
    char vocab_path[512];
    snprintf(vocab_path, sizeof(vocab_path), "%s/mojicast-punct-onnx/vocab.txt", models_dir);
    if (!punct_vocab_load(&engine->vocab, vocab_path)) return NULL;

    // 2. ONNX Runtime C-API の基底ポインタを取得
    engine->ort = OrtGetApiBase()->GetApi(ORT_API_VERSION);
    if (!engine->ort) return NULL;

    // 3. ログ環境 (OrtEnv) の作成
    OrtStatus *status = engine->ort->CreateEnv(ORT_LOGGING_LEVEL_WARNING, "ndwk_punct", &engine->env);
    if (status) { engine->ort->ReleaseStatus(status); return NULL; }

    // 4. セッション設定 (CPUスレッド数を2に指定)
    OrtSessionOptions *opts = NULL;
    ORT_IGNORE(engine->ort->CreateSessionOptions(&opts));
    ORT_IGNORE(engine->ort->SetIntraOpNumThreads(opts, 2));

    // 5. モデルのロード
    // ★まずは省メモリ＆高速な int8 動的量子化版 (109MB) を探す！
    char model_path[512];
    snprintf(model_path, sizeof(model_path), "%s/mojicast-punct-onnx/punct_bert.int8.onnx", models_dir);
    status = engine->ort->CreateSession(engine->env, model_path, opts, &engine->session);

    if (status) {
        // int8 ファイルが無い場合は、通常の fp32 版 (364MB) を試す
        engine->ort->ReleaseStatus(status);
        snprintf(model_path, sizeof(model_path), "%s/mojicast-punct-onnx/punct_bert.onnx", models_dir);
        status = engine->ort->CreateSession(engine->env, model_path, opts, &engine->session);
    }
    engine->ort->ReleaseSessionOptions(opts);

    if (status) {
        // どちらのモデルファイルも開けなかった場合
        engine->ort->ReleaseStatus(status);
        engine->ort->ReleaseEnv(engine->env);
        return NULL;
    }

    // 6. CPUメモリ情報 (OrtMemoryInfo) をキャッシュ
    // 推論のたびに CreateCpuMemoryInfo を呼ぶとメモリ割り当てオーバーヘッドが出るため、
    // エンジン生成時に1回だけ作って使い回します。
    ORT_IGNORE(engine->ort->CreateCpuMemoryInfo(OrtArenaAllocator, OrtMemTypeDefault, &engine->mem_info));
    return engine;
}

/**
 * @brief 句読点復元エンジンの破棄
 */
void punct_engine_destroy(punct_engine_t *engine) {
    if (!engine || !engine->ort) return;
    if (engine->mem_info) { engine->ort->ReleaseMemoryInfo(engine->mem_info); engine->mem_info = NULL; }
    if (engine->session)  { engine->ort->ReleaseSession(engine->session); engine->session = NULL; }
    if (engine->env)      { engine->ort->ReleaseEnv(engine->env); engine->env = NULL; }
}

/**
 * @brief 平文テキストに句読点を復元して返す (メインAPI)
 *
 * 【処理の流れ】
 * ① トークナイザで UTF-8 文字分解 ＆ input_ids 配列の生成
 * ② スタック配列をゼロコピーで ONNX テンソルにラップして BERT 推論を実行
 * ③ 得られた各文字のロジットをフォーマッタに渡し、日本語ルールに従って文字列を合成
 *
 * @param engine エンジンポインタ
 * @param text   句読点のないテキスト (UTF-8)
 * @return 句読点が付与されたテキスト (内部静的バッファ output_buffer のポインタなので free 不要)
 */
const char *punct_engine_restore(punct_engine_t *engine, const char *text) {
    if (unlikely(!engine || !engine->session || !text || text[0] == '\0')) {
        return text;
    }

    // スタック上にテンソル入力バッファを用意 (malloc完全ゼロ！)
    char_span_t chars[MAX_PUNCT_SEQ_LEN];
    int64_t input_ids[MAX_PUNCT_SEQ_LEN];
    int64_t attention_mask[MAX_PUNCT_SEQ_LEN];

    // ---------------------------------------------------------------------
    // ① [責務: トークナイザ] 文字分解 ＆ トークンID生成
    // ---------------------------------------------------------------------
    size_t num_chars = punct_tokenize(
        &engine->vocab, text, chars, input_ids, attention_mask, MAX_PUNCT_SEQ_LEN);
    if (num_chars == 0) return text;

    // ---------------------------------------------------------------------
    // ② [責務: ONNX 推論] テンソル作成 ＆ BERT 推論実行
    // ---------------------------------------------------------------------
    size_t seq_len = num_chars + 2; // [CLS] + 文字数 + [SEP]
    int64_t shape[2] = {1, (int64_t)seq_len}; // バッチサイズ 1 × 系列長
    OrtValue *in_ids_tensor = NULL;
    OrtValue *in_mask_tensor = NULL;

    // スタック配列のポインタをそのまま使ってゼロコピーでテンソルを作成
    // ※ seq_len * sizeof(int64_t) の代わりにビットシフト (<< NDWK_INT64_SHIFT)
    ORT_IGNORE(engine->ort->CreateTensorWithDataAsOrtValue(
        engine->mem_info, input_ids, seq_len << NDWK_INT64_SHIFT,
        shape, 2, ONNX_TENSOR_ELEMENT_DATA_TYPE_INT64, &in_ids_tensor));

    ORT_IGNORE(engine->ort->CreateTensorWithDataAsOrtValue(
        engine->mem_info, attention_mask, seq_len << NDWK_INT64_SHIFT,
        shape, 2, ONNX_TENSOR_ELEMENT_DATA_TYPE_INT64, &in_mask_tensor));

    const char *input_names[] = {"input_ids", "attention_mask"};
    const OrtValue *input_tensors[] = {in_ids_tensor, in_mask_tensor};
    const char *output_names[] = {"logits"};
    OrtValue *out_tensor = NULL;

    // BERT 推論実行！
    OrtStatus *status = engine->ort->Run(
        engine->session, NULL, input_names, input_tensors, 2, output_names, 1, &out_tensor);

    // 入力テンソルオブジェクトは推論完了直後に即破棄
    engine->ort->ReleaseValue(in_mask_tensor);
    engine->ort->ReleaseValue(in_ids_tensor);

    if (status) {
        // 推論エラー時は安全に平文のまま返す
        engine->ort->ReleaseStatus(status);
        return text;
    }

    // 出力テンソルから生ロジット配列のポインタを取り出す
    float *logits = NULL;
    ORT_IGNORE(engine->ort->GetTensorMutableData(out_tensor, (void **)&logits));

    // ---------------------------------------------------------------------
    // ③ [責務: フォーマッタ] 日本語文法・句読点ルール整形
    // ---------------------------------------------------------------------
    punct_format_text(chars, num_chars, logits, engine->output_buffer, sizeof(engine->output_buffer));

    // 出力テンソルの解放
    engine->ort->ReleaseValue(out_tensor);

    return engine->output_buffer;
}
