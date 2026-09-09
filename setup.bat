@echo off
setlocal enabledelayedexpansion

cd /d "%~dp0"

echo ==================================================
echo  ndwk Environment ^& Model Setup Script (Windows)
echo ==================================================

set DOWNLOAD_ALL=false
if /i "%~1"=="--all" set DOWNLOAD_ALL=true
if /i "%~1"=="all" set DOWNLOAD_ALL=true
if /i "%~1"=="-a" set DOWNLOAD_ALL=true

if "%DOWNLOAD_ALL%"=="true" (
    echo [Mode] Full multilingual setup (All models)
) else (
    echo [Mode] Minimal Japanese setup (Pass --all for all languages)
)

if not exist "Lib" mkdir Lib
if not exist "models" mkdir models

:: 1. Lib\dr_wav.h
if not exist "Lib\dr_wav.h" (
    echo [1/3] Downloading Lib\dr_wav.h...
    curl.exe -fSL --progress-bar -o Lib\dr_wav.h "https://raw.githubusercontent.com/mackron/dr_libs/master/dr_wav.h"
) else (
    echo [1/3] Lib\dr_wav.h already exists. Skipping.
)

:: 2. Lib\miniaudio.h
if not exist "Lib\miniaudio.h" (
    echo [2/3] Downloading Lib\miniaudio.h...
    curl.exe -fSL --progress-bar -o Lib\miniaudio.h "https://raw.githubusercontent.com/mackron/miniaudio/master/miniaudio.h"
) else (
    echo [2/3] Lib\miniaudio.h already exists. Skipping.
)

:: 3. Lib\sherpa-onnx (C-API binary for Windows x64)
if not exist "Lib\sherpa-onnx" (
    echo [3/3] Downloading sherpa-onnx C-API library for Windows...
    set SHERPA_VER=v1.10.45
    set ARCHIVE=sherpa-onnx-!SHERPA_VER!-win-x64.zip
    set URL=https://github.com/k2-fsa/sherpa-onnx/releases/download/!SHERPA_VER!/!ARCHIVE!
    echo Downloading from: !URL!
    curl.exe -fSL --progress-bar -o "Lib\!ARCHIVE!" "!URL!"
    echo Extracting...
    tar.exe -xf "Lib\!ARCHIVE!" -C Lib\
    for /d %%D in (Lib\sherpa-onnx-!SHERPA_VER!*) do (
        ren "%%D" sherpa-onnx
    )
    del /f /q "Lib\!ARCHIVE!"
    echo sherpa-onnx setup completed.
) else (
    echo [3/3] Lib\sherpa-onnx already exists. Skipping.
)

echo.
echo ==================================================
echo  Downloading Models...
echo ==================================================

:: Silero VAD
if not exist "models\silero_vad.onnx" (
    echo [-] Downloading silero_vad.onnx...
    curl.exe -fSL --progress-bar -o models\silero_vad.onnx "https://github.com/k2-fsa/sherpa-onnx/releases/download/asr-models/silero_vad.onnx"
) else (
    echo [-] models\silero_vad.onnx already exists.
)

:: Japanese ReazonSpeech Zipformer
if not exist "models\sherpa-onnx-zipformer-ja-en-reazonspeech-2025-01-17" (
    echo [-] Downloading sherpa-onnx-zipformer-ja-en-reazonspeech-2025-01-17...
    curl.exe -fSL --progress-bar -o models\zipformer-ja.tar.bz2 "https://github.com/k2-fsa/sherpa-onnx/releases/download/asr-models/sherpa-onnx-zipformer-ja-en-reazonspeech-2025-01-17.tar.bz2"
    tar.exe -xf models\zipformer-ja.tar.bz2 -C models\
    del /f /q models\zipformer-ja.tar.bz2
) else (
    echo [-] models\sherpa-onnx-zipformer-ja-en-reazonspeech-2025-01-17 already exists.
)

if "%DOWNLOAD_ALL%"=="true" (
    :: Whisper Tiny
    if not exist "models\sherpa-onnx-whisper-tiny" (
        echo [-] Downloading sherpa-onnx-whisper-tiny...
        curl.exe -fSL --progress-bar -o models\whisper-tiny.tar.bz2 "https://github.com/k2-fsa/sherpa-onnx/releases/download/asr-models/sherpa-onnx-whisper-tiny.tar.bz2"
        tar.exe -xf models\whisper-tiny.tar.bz2 -C models\
        del /f /q models\whisper-tiny.tar.bz2
    ) else (
        echo [-] models\sherpa-onnx-whisper-tiny already exists.
    )

    :: Paraformer zh
    if not exist "models\sherpa-onnx-paraformer-zh-int8-2025-10-07" (
        echo [-] Downloading sherpa-onnx-paraformer-zh-int8-2025-10-07...
        curl.exe -fSL --progress-bar -o models\paraformer-zh.tar.bz2 "https://github.com/k2-fsa/sherpa-onnx/releases/download/asr-models/sherpa-onnx-paraformer-zh-int8-2025-10-07.tar.bz2"
        tar.exe -xf models\paraformer-zh.tar.bz2 -C models\
        del /f /q models\paraformer-zh.tar.bz2
    ) else (
        echo [-] models\sherpa-onnx-paraformer-zh-int8-2025-10-07 already exists.
    )

    :: SenseVoice
    if not exist "models\sherpa-onnx-sense-voice-zh-en-ja-ko-yue-int8-2024-07-17" (
        echo [-] Downloading sherpa-onnx-sense-voice...
        curl.exe -fSL --progress-bar -o models\sense-voice.tar.bz2 "https://github.com/k2-fsa/sherpa-onnx/releases/download/asr-models/sherpa-onnx-sense-voice-zh-en-ja-ko-yue-int8-2024-07-17.tar.bz2"
        tar.exe -xf models\sense-voice.tar.bz2 -C models\
        del /f /q models\sense-voice.tar.bz2
    ) else (
        echo [-] models\sherpa-onnx-sense-voice already exists.
    )

    :: Parakeet TDT en
    if not exist "models\sherpa-onnx-nemo-parakeet-tdt-0.6b-v3-int8" (
        echo [-] Downloading sherpa-onnx-nemo-parakeet-tdt...
        curl.exe -fSL --progress-bar -o models\parakeet-en.tar.bz2 "https://github.com/k2-fsa/sherpa-onnx/releases/download/asr-models/sherpa-onnx-nemo-parakeet-tdt-0.6b-v3-int8.tar.bz2"
        tar.exe -xf models\parakeet-en.tar.bz2 -C models\
        del /f /q models\parakeet-en.tar.bz2
    ) else (
        echo [-] models\sherpa-onnx-nemo-parakeet-tdt already exists.
    )

    :: Mojicast Punctuation (BERT)
    if not exist "models\mojicast-punct-onnx" (
        echo [-] Downloading mojicast-punct-onnx...
        mkdir models\mojicast-punct-onnx
        curl.exe -fSL --progress-bar -o models\mojicast-punct-onnx\punct_bert.onnx "https://huggingface.co/ishiki-emo/mojicast-punct-onnx/resolve/main/punct_bert.onnx"
        curl.exe -fSL --progress-bar -o models\mojicast-punct-onnx\vocab.txt "https://huggingface.co/ishiki-emo/mojicast-punct-onnx/resolve/main/vocab.txt"
        curl.exe -fSL --progress-bar -o models\mojicast-punct-onnx\README.md "https://huggingface.co/ishiki-emo/mojicast-punct-onnx/resolve/main/README.md"
    ) else (
        echo [-] models\mojicast-punct-onnx already exists.
    )
)

echo.
echo ==================================================
echo  Setup completed successfully!
echo  Next steps:
echo    cmake -B build
echo    cmake --build build
echo    build\ndwk.exe --mic
echo ==================================================
