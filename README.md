# ndwk

A lightweight, zero-delay, CPU-only real-time multilingual speech recognition (ASR) engine implemented in pure C11.  
Designed for minimal resource footprint, edge Linux, and embedded microcontroller targets without heavy runtime dependencies like Python.

[English](#english) | [日本語](#日本語)

---

## English

### Key Features

- **Pure C11 Core Engine**: No Python or heavy runtime dependencies. Engineered with single-responsibility modular architecture for portability to embedded and edge Linux devices (`libndwk.a`, `libndwk.so`).
- **Multi-Language SDK**: First-class, zero-dependency bindings for **C# (.NET 10)** and **Rust** with idiomatic, type-safe APIs.
- **Low Memory Footprint**: Bounded resident RAM usage (~174MB). Zero dynamic memory allocation (`malloc=0`) in real-time streaming loops.
- **Live Microphone Input**: Real-time microphone capture across Linux, WSL2, and Windows via single-header `miniaudio`.
- **Zero-Delay Streaming**: Monotonic clock drift compensation ensures exact real-time playback synchronization, printing instant partial drafts via terminal in-place overwrite.
- **Punctuation Restoration**: Built-in lightweight Japanese punctuation restoration engine, automatically predicting commas, periods, and question marks in real-time.
- **Audio Pre-roll (Head-dropout Prevention)**: Maintains a 10-second sliding history buffer, seamlessly prepending up to 1.0 second of pre-speech audio to eliminate VAD onset clipping.
- **Multilingual Routing**:
  - **Japanese**: ReazonSpeech Zipformer (int8)
  - **English**: NeMo Parakeet TDT v3 (int8)
  - **Chinese**: Paraformer-large (int8)
  - **Korean / Multilingual**: SenseVoice Small (int8)
  - **Automatic Language Identification**: Whisper-tiny LID

### Prerequisites

- **CMake**: Version 3.15 or newer
- **C Compiler**: C11-compliant compiler (GCC 9+, Clang 10+, MSVC 2019+)
- **Build Tool**: Ninja (recommended) or Make
- **Supported Platforms**: Linux (x86_64, aarch64), WSL2 (Ubuntu 20.04+), Windows 10/11, macOS
- *(Optional)* **.NET 10 SDK**: For C# bindings
- *(Optional)* **Rust / Cargo**: For Rust bindings

### Quick Start

#### 1. Clone Repository

```bash
git clone https://github.com/harunya0/ndwk.git
cd ndwk
```

#### 2. Download Dependencies and Models

Run the setup script to automatically fetch single-header libraries (`dr_wav.h`, `miniaudio.h`), the `sherpa-onnx` C-API shared library, and pre-trained ONNX models.

**Linux / WSL2 / macOS:**
```bash
# Minimal setup (Japanese ReazonSpeech + Silero VAD, ~500MB)
./setup.sh

# Full multilingual setup (All supported language models)
./setup.sh --all
```

**Windows (Command Prompt / PowerShell):**
```cmd
:: Minimal setup
setup.bat

:: Full multilingual setup
setup.bat --all
```

#### 3. Build

```bash
cmake -B build -G Ninja
cmake --build build
```
*(On Windows with MSVC: `cmake -B build && cmake --build build --config Release`)*

This produces:
- `build/libndwk.a`: Static core library (213 KB)
- `build/libndwk.so`: Shared core library (27 KB)
- `build/ndwk`: Standalone CLI executable (307 KB)

### Usage

#### Live Microphone Mode

```bash
# Auto language detection (default)
./build/ndwk --mic

# Manual language selection (conserves memory by loading only one ASR model)
./build/ndwk --mic ja   # Japanese
./build/ndwk --mic en   # English
./build/ndwk --mic zh   # Chinese
./build/ndwk --mic ko   # Korean
```

*Note for WSL2 users: If audio is routed through WSLg PulseAudio, run with:*
```bash
PULSE_SERVER=unix:/mnt/wslg/runtime-dir/pulse/native ./build/ndwk --mic
```

#### Offline WAV File Mode

Input files must be 16kHz, 16-bit mono PCM WAV:

```bash
./build/ndwk test/ja/ja_014.wav ja
```

#### Language Bindings (SDK)

ndwk can be integrated into high-level languages with zero external dependencies via its clean C-ABI:

**C# (.NET 10 / Unity):**
```bash
dotnet run --project bindings/csharp/Ndwk.Sample/Ndwk.Sample.csproj test/ja/ja_033.wav models
```

**Rust:**
```bash
cargo run --manifest-path bindings/rust/Cargo.toml -- test/ja/ja_033.wav models
```

### Architecture

```text
[ main.c ] (CLI)      [ C# (.NET 10) ]      [ Rust ]
    |                        |                 |
    +------------------------+-----------------+
                             | (C-ABI: Inc/ndwk.h)
                             v
                    [ libndwk.so / .a ]
                             |
         +-------------------+-------------------+
         |                   |                   |
   [ vad_detector ]    [ asr_engine ]     [ punct_engine ]
    (Silero VAD)       (Zipformer/etc)     (Rule+CharLM)
         |                   |
   [ audio_history ]   [ model_config ]
```

### Credits & Acknowledgments

- **ASR & VAD Engine**: [sherpa-onnx](https://github.com/k2-fsa/sherpa-onnx) (Apache-2.0)
- **Japanese ASR Model**: [ReazonSpeech](https://github.com/reazon-research/ReazonSpeech) (Apache-2.0)
- **Audio Capture**: [miniaudio](https://github.com/mackron/miniaudio) (Public Domain / MIT-0)
- **WAV Decoding**: [dr_wav](https://github.com/mackron/dr_libs) (Public Domain / MIT-0)
- **VAD Model**: [Silero VAD](https://github.com/snakers4/silero-vad) (MIT)
- **Punctuation Model**: [Mojicast](https://github.com/ishiki-emo/mojicast) (Apache-2.0)

---

## 日本語

### 主な特徴

- **純C11コアエンジン**: Python等の重量なランタイムに依存せず、C11 + CMake + Ninja で構築。組み込みLinuxやマイコン環境への移植を考慮したコアライブラリ（`libndwk.a`, `libndwk.so`）構成。
- **多言語SDK対応**: 外部依存ゼロ（0 dependencies）の **C# (.NET 10)** および **Rust** 公式バインディングを同梱。
- **超低メモリ消費**: 常駐メモリは約174MB。リアルタイム推論ループ内での不要な動的メモリ確保（`malloc=0`）を徹底排除。
- **ライブマイク入力**: `miniaudio` を採用し、Linux / WSL2 / Windows においてクロスプラットフォームでリアルタイム録音に対応。
- **ゼロ遅延ストリーミング**: 単調増加クロック（CLOCK_MONOTONIC）による実時間ドリフト補正を行い、端末行上書きによるリアルタイム速報字幕（Partial）表示を実現。
- **自動句読点復元**: 日本語音声認識に特化した軽量句読点復元エンジンを内蔵し、「、」「。」「？」をリアルタイム自動付与。
- **頭切れ防止（プリロール機能）**: 直近10秒のスライディングバッファを備え、VAD検知直前の実音声（最大1.0秒）を自動結合して発話語頭の母音欠落を完全に防止。
- **多言語ハイブリッドルーティング**:
  - **日本語**: ReazonSpeech Zipformer (int8)
  - **英語**: NeMo Parakeet TDT v3 (int8)
  - **中国語**: Paraformer-large (int8)
  - **韓国語・多言語**: SenseVoice Small (int8)
  - **自動言語識別 (LID)**: Whisper-tiny による発話ごとの高速言語判定

### 必要要件

- **CMake**: 3.15 以上
- **Cコンパイラ**: C11 をサポートするコンパイラ (GCC 9+, Clang 10+, MSVC 2019+)
- **ビルドツール**: Ninja (推奨) または Make
- **対応OS**: Linux (x86_64, aarch64), WSL2 (Ubuntu 20.04+), Windows 10/11, macOS
- *(任意)* **.NET 10 SDK**: C# バインディング用
- *(任意)* **Rust / Cargo**: Rust バインディング用

### クイックスタート

#### 1. リポジトリのクローン

```bash
git clone https://github.com/harunya0/ndwk.git
cd ndwk
```

#### 2. 依存関係とモデルの取得

セットアップスクリプトを実行すると、シングルヘッダ（`dr_wav.h`, `miniaudio.h`）、`sherpa-onnx` C-API 共有ライブラリ、および学習済み ONNX モデルが自動ダウンロード・展開されます。

**Linux / WSL2 / macOS:**
```bash
# 最小構成 (日本語 ReazonSpeech + Silero VAD / 約500MB)
./setup.sh

# 全言語モデルを含むフル構成
./setup.sh --all
```

**Windows (コマンドプロンプト / PowerShell):**
```cmd
:: 最小構成
setup.bat

:: 全言語フル構成
setup.bat --all
```

#### 3. ビルド

```bash
cmake -B build -G Ninja
cmake --build build
```
*(Windows で MSVC を使用する場合: `cmake -B build && cmake --build build --config Release`)*

ビルドにより以下の成果物が生成されます:
- `build/libndwk.a`: 静的コアライブラリ (213 KB)
- `build/libndwk.so`: 共有コアライブラリ (27 KB)
- `build/ndwk`: スタンドアロン CLI 実行ファイル (307 KB)

### 使い方

#### マイク入力モード (リアルタイム認識)

```bash
# 自動言語判定モード (デフォルト)
./build/ndwk --mic

# 言語固定モード (ASRモデルを1つだけロードしメモリを節約)
./build/ndwk --mic ja   # 日本語
./build/ndwk --mic en   # 英語
./build/ndwk --mic zh   # 中国語
./build/ndwk --mic ko   # 韓国語
```

*WSL2 環境でマイクを使用する場合の注意:*  
WSLg の PulseAudio ソケットを指定して実行してください:  
```bash
PULSE_SERVER=unix:/mnt/wslg/runtime-dir/pulse/native ./build/ndwk --mic
```

#### オフライン WAV ファイル認識モード

入力ファイルは 16kHz / 16-bit モノラル WAV である必要があります:

```bash
./build/ndwk test/ja/ja_014.wav ja
```

#### 言語バインディング (SDK)

C言語で提供される `ndwk.h` / `libndwk.so` を通じて、他言語から外部ライブラリ依存ゼロで組み込み可能です。

**C# (.NET 10 / Unity):**
```bash
dotnet run --project bindings/csharp/Ndwk.Sample/Ndwk.Sample.csproj test/ja/ja_033.wav models
```

**Rust:**
```bash
cargo run --manifest-path bindings/rust/Cargo.toml -- test/ja/ja_033.wav models
```

### アーキテクチャ構成

クリーンアーキテクチャの原則に基づき、コアエンジンとインターフェースが完全に分離されています:

```text
[ main.c ] (CLI)      [ C# (.NET 10) ]      [ Rust ]
    |                        |                 |
    +------------------------+-----------------+
                             | (C-ABI: Inc/ndwk.h)
                             v
                    [ libndwk.so / .a ]
                             |
         +-------------------+-------------------+
         |                   |                   |
   [ vad_detector ]    [ asr_engine ]     [ punct_engine ]
    (Silero VAD)       (Zipformer/etc)     (Rule+CharLM)
         |                   |
   [ audio_history ]   [ model_config ]
```

### ディレクトリ構成

```text
ndwk/
├── CMakeLists.txt        # ビルド定義 (C11, libndwk.a / libndwk.so / ndwk)
├── setup.sh              # Linux/macOS 用自動セットアップスクリプト
├── setup.bat             # Windows 用自動セットアップスクリプト
├── README.md             # 本書
├── Inc/                  # 公開・内部ヘッダーファイル
│   ├── ndwk.h            # 公開 C-ABI ヘッダー
│   ├── config.h          # チューニング可能定数 (閾値, スレッド数, 秒数)
│   ├── ndwk_types.h      # 言語 enum などの共通型定義
│   ├── mic_reader.h      # マイクキャプチャ
│   ├── vad_detector.h    # VAD
│   ├── audio_history.h   # スライディングバッファ
│   ├── asr_engine.h      # 音声認識エンジン
│   ├── lang_detector.h   # 言語判定
│   ├── model_config.h    # モデル設定
│   └── punct_engine.h    # 句読点復元エンジン
├── Src/                  # 実装コード (ndwk.c, main.c, ...)
├── bindings/             # 多言語バインディング
│   ├── csharp/           # C# (.NET 10) P/Invoke バインディング
│   └── rust/             # Rust ゼロコスト FFI バインディング
├── Lib/                  # サードパーティライブラリ (setup スクリプトで配置)
├── models/               # ONNX モデル (setup スクリプトで配置)
├── test/                 # テスト用音声ファイル
└── tmp/                  # 技術解説ドキュメント
```

### ライセンス

本リポジトリのソースコードは [Apache-2.0 License](LICENSE) の下で公開されています。  
各学習済みモデルおよびサードパーティライブラリのライセンスはそれぞれの配布元に従います。\n