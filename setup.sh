#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

echo "=================================================="
echo " ndwk Environment & Model Setup Script (Linux/macOS)"
echo "=================================================="

DOWNLOAD_ALL=false
if [[ "${1:-}" == "--all" || "${1:-}" == "-a" ]]; then
    DOWNLOAD_ALL=true
    echo "[Mode] Full multilingual setup (All models)"
else
    echo "[Mode] Minimal Japanese setup (Pass --all for all languages)"
fi

mkdir -p Lib models

# 1. Lib/dr_wav.h
if [[ ! -f Lib/dr_wav.h ]]; then
    echo "[1/3] Downloading Lib/dr_wav.h..."
    curl -fSL --progress-bar -o Lib/dr_wav.h "https://raw.githubusercontent.com/mackron/dr_libs/master/dr_wav.h"
else
    echo "[1/3] Lib/dr_wav.h already exists. Skipping."
fi

# 2. Lib/miniaudio.h
if [[ ! -f Lib/miniaudio.h ]]; then
    echo "[2/3] Downloading Lib/miniaudio.h..."
    curl -fSL --progress-bar -o Lib/miniaudio.h "https://raw.githubusercontent.com/mackron/miniaudio/master/miniaudio.h"
else
    echo "[2/3] Lib/miniaudio.h already exists. Skipping."
fi

# 3. Lib/sherpa-onnx (C-API binary)
if [[ ! -d Lib/sherpa-onnx ]]; then
    echo "[3/3] Downloading sherpa-onnx C-API library..."
    OS="$(uname -s)"
    ARCH="$(uname -m)"
    SHERPA_VER="v1.10.45"
    
    if [[ "$OS" == "Linux" && "$ARCH" == "x86_64" ]]; then
        ARCHIVE="sherpa-onnx-${SHERPA_VER}-linux-x64.tar.bz2"
    elif [[ "$OS" == "Linux" && ("$ARCH" == "aarch64" || "$ARCH" == "arm64") ]]; then
        ARCHIVE="sherpa-onnx-${SHERPA_VER}-linux-arm64.tar.bz2"
    elif [[ "$OS" == "Darwin" && "$ARCH" == "arm64" ]]; then
        ARCHIVE="sherpa-onnx-${SHERPA_VER}-osx-arm64.tar.bz2"
    elif [[ "$OS" == "Darwin" && "$ARCH" == "x86_64" ]]; then
        ARCHIVE="sherpa-onnx-${SHERPA_VER}-osx-x64.tar.bz2"
    else
        echo "Error: Unsupported OS/Architecture: $OS $ARCH"
        exit 1
    fi

    URL="https://github.com/k2-fsa/sherpa-onnx/releases/download/${SHERPA_VER}/${ARCHIVE}"
    echo "Downloading from: $URL"
    curl -fSL --progress-bar -o "Lib/${ARCHIVE}" "$URL"
    echo "Extracting..."
    tar -xjf "Lib/${ARCHIVE}" -C Lib/
    EXTRACTED_DIR=$(find Lib -maxdepth 1 -type d -name "sherpa-onnx-${SHERPA_VER}*" | head -n 1)
    if [[ -n "$EXTRACTED_DIR" ]]; then
        mv "$EXTRACTED_DIR" Lib/sherpa-onnx
    fi
    rm -f "Lib/${ARCHIVE}"
    echo "sherpa-onnx setup completed."
else
    echo "[3/3] Lib/sherpa-onnx already exists. Skipping."
fi

echo ""
echo "=================================================="
echo " Downloading Models..."
echo "=================================================="

# Silero VAD
if [[ ! -f models/silero_vad.onnx ]]; then
    echo "[-] Downloading silero_vad.onnx..."
    curl -fSL --progress-bar -o models/silero_vad.onnx \
        "https://github.com/k2-fsa/sherpa-onnx/releases/download/asr-models/silero_vad.onnx"
else
    echo "[-] models/silero_vad.onnx already exists."
fi

# Japanese ReazonSpeech Zipformer
if [[ ! -d models/sherpa-onnx-zipformer-ja-en-reazonspeech-2025-01-17 ]]; then
    echo "[-] Downloading sherpa-onnx-zipformer-ja-en-reazonspeech-2025-01-17..."
    curl -fSL --progress-bar -o models/zipformer-ja.tar.bz2 \
        "https://github.com/k2-fsa/sherpa-onnx/releases/download/asr-models/sherpa-onnx-zipformer-ja-en-reazonspeech-2025-01-17.tar.bz2"
    tar -xjf models/zipformer-ja.tar.bz2 -C models/
    rm -f models/zipformer-ja.tar.bz2
else
    echo "[-] models/sherpa-onnx-zipformer-ja-en-reazonspeech-2025-01-17 already exists."
fi

if [[ "$DOWNLOAD_ALL" == true ]]; then
    # Whisper Tiny (LID)
    if [[ ! -d models/sherpa-onnx-whisper-tiny ]]; then
        echo "[-] Downloading sherpa-onnx-whisper-tiny..."
        curl -fSL --progress-bar -o models/whisper-tiny.tar.bz2 \
            "https://github.com/k2-fsa/sherpa-onnx/releases/download/asr-models/sherpa-onnx-whisper-tiny.tar.bz2"
        tar -xjf models/whisper-tiny.tar.bz2 -C models/
        rm -f models/whisper-tiny.tar.bz2
    else
        echo "[-] models/sherpa-onnx-whisper-tiny already exists."
    fi

    # Paraformer zh
    if [[ ! -d models/sherpa-onnx-paraformer-zh-int8-2025-10-07 ]]; then
        echo "[-] Downloading sherpa-onnx-paraformer-zh-int8-2025-10-07..."
        curl -fSL --progress-bar -o models/paraformer-zh.tar.bz2 \
            "https://github.com/k2-fsa/sherpa-onnx/releases/download/asr-models/sherpa-onnx-paraformer-zh-int8-2025-10-07.tar.bz2"
        tar -xjf models/paraformer-zh.tar.bz2 -C models/
        rm -f models/paraformer-zh.tar.bz2
    else
        echo "[-] models/sherpa-onnx-paraformer-zh-int8-2025-10-07 already exists."
    fi

    # SenseVoice
    if [[ ! -d models/sherpa-onnx-sense-voice-zh-en-ja-ko-yue-int8-2024-07-17 ]]; then
        echo "[-] Downloading sherpa-onnx-sense-voice..."
        curl -fSL --progress-bar -o models/sense-voice.tar.bz2 \
            "https://github.com/k2-fsa/sherpa-onnx/releases/download/asr-models/sherpa-onnx-sense-voice-zh-en-ja-ko-yue-int8-2024-07-17.tar.bz2"
        tar -xjf models/sense-voice.tar.bz2 -C models/
        rm -f models/sense-voice.tar.bz2
    else
        echo "[-] models/sherpa-onnx-sense-voice already exists."
    fi

    # Parakeet TDT en
    if [[ ! -d models/sherpa-onnx-nemo-parakeet-tdt-0.6b-v3-int8 ]]; then
        echo "[-] Downloading sherpa-onnx-nemo-parakeet-tdt..."
        curl -fSL --progress-bar -o models/parakeet-en.tar.bz2 \
            "https://github.com/k2-fsa/sherpa-onnx/releases/download/asr-models/sherpa-onnx-nemo-parakeet-tdt-0.6b-v3-int8.tar.bz2"
        tar -xjf models/parakeet-en.tar.bz2 -C models/
        rm -f models/parakeet-en.tar.bz2
    else
        echo "[-] models/sherpa-onnx-nemo-parakeet-tdt already exists."
    fi

    # Mojicast Punctuation (BERT)
    if [[ ! -d models/mojicast-punct-onnx ]]; then
        echo "[-] Downloading mojicast-punct-onnx..."
        mkdir -p models/mojicast-punct-onnx
        curl -fSL --progress-bar -o models/mojicast-punct-onnx/punct_bert.onnx \
            "https://huggingface.co/ishiki-emo/mojicast-punct-onnx/resolve/main/punct_bert.onnx"
        curl -fSL --progress-bar -o models/mojicast-punct-onnx/vocab.txt \
            "https://huggingface.co/ishiki-emo/mojicast-punct-onnx/resolve/main/vocab.txt"
        curl -fSL --progress-bar -o models/mojicast-punct-onnx/README.md \
            "https://huggingface.co/ishiki-emo/mojicast-punct-onnx/resolve/main/README.md"
    else
        echo "[-] models/mojicast-punct-onnx already exists."
    fi
fi

echo ""
echo "=================================================="
echo " Setup completed successfully!"
echo " Next steps:"
echo "   cmake -B build -G Ninja"
echo "   cmake --build build"
echo "   ./build/ndwk --mic"
echo "=================================================="
