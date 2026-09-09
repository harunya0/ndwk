# ndwk

A lightweight, zero-delay, CPU-only real-time multilingual speech recognition (ASR) engine implemented in pure C11.  
Designed for minimal resource footprint, edge Linux, and embedded microcontroller targets without heavy runtime dependencies like Python.

[English](#english) | [日本語](#日本語)

---

## English

### Key Features

- **Pure C11 Implementation**: No Python or heavy runtime dependencies. Engineered with single-responsibility modular architecture for portability to embedded and edge Linux devices.
- **Low Memory Footprint**: Bounded resident RAM usage (under ~200MB). Avoids continuous heap allocations in real-time streaming loops.
- **Live Microphone Input**: Real-time microphone capture across Linux, WSL2, and Windows via single-header `miniaudio`.
- **Zero-Delay Streaming**: Monotonic clock drift compensation ensures exact real-time playback synchronization, printing instant partial drafts via terminal in-place overwrite.
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

### Architecture

The codebase adheres to Clean Architecture and the Single Responsibility Principle:

```text
[ main.c ]  ----------------------- Entry point and CLI argument parser
    |
    v
[ pipeline ]  --------------------- Pipeline orchestrator & monotonic clock sync
    |
    +-- [ mic_reader ]  ----------- Thread-safe microphone capture via miniaudio
    +-- [ wav_reader ]  ----------- WAV file decoder via dr_wav
    +-- [ audio_history ]  -------- 10-second sliding buffer with audio pre-roll
    +-- [ vad_detector ]  --------- Voice activity detection via Silero VAD
    +-- [ lang_detector ]  -------- Spoken language identification via Whisper-tiny
    +-- [ asr_engine ]  ----------- Speech-to-text inference engine
            |
            +-- [ model_config ]  - Model path routing and hyperparameter builder
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

- **純C11実装**: Python等の重量なランタイムに依存せず、C11 + CMake + Ninja で構築。組み込みLinuxやマイコン環境への移植を考慮した単一責任設計を採用。
- **低メモリ消費**: 常駐メモリは約200MB以下。リアルタイム推論ループ内での不要な動的メモリ確保（malloc）を排除。
- **ライブマイク入力**: `miniaudio` を採用し、Linux / WSL2 / Windows においてクロスプラットフォームでリアルタイム録音に対応。
- **ゼロ遅延ストリーミング**: 単調増加クロック（CLOCK_MONOTONIC）による実時間ドリフト補正を行い、端末行上書きによるリアルタイム速報字幕（Partial）表示を実現。
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

### アーキテクチャ構成

クリーンアーキテクチャの原則に基づき、各モジュールの責務が明確に分離されています:

```text
[ main.c ]  ----------------------- エントリポイントおよびCLI引数解析
    |
    v
[ pipeline ]  --------------------- パイプライン統括および実時間クロック同期
    |
    +-- [ mic_reader ]  ----------- miniaudio によるスレッドセーフな音声キャプチャ
    +-- [ wav_reader ]  ----------- dr_wav による WAV デコード
    +-- [ audio_history ]  -------- 10秒スライディングバッファおよびプリロール結合
    +-- [ vad_detector ]  --------- Silero VAD による発話区間検出
    +-- [ lang_detector ]  -------- Whisper-tiny による言語自動判別
    +-- [ asr_engine ]  ----------- 音声認識・テキスト変換推論
            |
            +-- [ model_config ]  - 各言語の ONNX パスおよび推論パラメータ構築
```

### ディレクトリ構成

```text
ndwk/
├── CMakeLists.txt        # ビルド定義 (C11, -Wall -Wextra)
├── setup.sh              # Linux/macOS 用自動セットアップスクリプト
├── setup.bat             # Windows 用自動セットアップスクリプト
├── README.md             # 本書
├── Inc/                  # ヘッダーファイル
│   ├── config.h          # チューニング可能定数 (閾値, スレッド数, 秒数)
│   ├── ndwk_types.h      # 言語 enum などの共通型定義
│   ├── pipeline.h        # パイプライン制御
│   ├── mic_reader.h      # マイクキャプチャ
│   ├── vad_detector.h    # VAD
│   ├── audio_history.h   # スライディングバッファ
│   ├── asr_engine.h      # 音声認識エンジン
│   ├── lang_detector.h   # 言語判定
│   └── model_config.h    # モデル設定
├── Src/                  # 実装コード (.c)
├── Lib/                  # サードパーティライブラリ (setup スクリプトで配置)
├── models/               # ONNX モデル (setup スクリプトで配置)
└── test/                 # テスト用音声ファイル
```

### ライセンス

本リポジトリのソースコードは [Apache-2.0 License](LICENSE) の下で公開されています。  
各学習済みモデルおよびサードパーティライブラリのライセンスはそれぞれの配布元に従います。
