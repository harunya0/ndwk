cmake_minimum_required(VERSION 3.15)
project(ndwk VERSION 0.1.0 LANGUAGES C)

# C11 標準を強制
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# コンパイル・リンク最適化オプション
add_compile_options(
    -Wall -Wextra -O3 -flto -march=native
    -fno-math-errno
    -fno-trapping-math
    -fomit-frame-pointer
    -ffunction-sections -fdata-sections
)
add_link_options(-flto -O3)
add_link_options(-Wl,--gc-sections)
add_link_options(-s)

# マイコン向け低RAMモードの切り替えオプション
option(NDWK_LOW_RAM "Enable Low-RAM profile for microcontrollers" OFF)
if(NDWK_LOW_RAM)
    add_compile_definitions(NDWK_PROFILE_LOW_RAM)
    message(STATUS "Build profile: LOW-RAM (Microcontroller profile)")
else()
    message(STATUS "Build profile: STANDARD (Desktop profile)")
endif()

# コアライブラリ
set(CORE_SOURCES
    Src/ndwk.c
    Src/asr_engine.c
    Src/vad_detector.c
    Src/audio_history.c
    Src/model_config.c
    Src/punct_engine.c
    Src/lang_detector.c
)

# 静的ライブラリ (libndwk.a)
add_library(ndwk_static STATIC ${CORE_SOURCES})

set_target_properties(ndwk_static PROPERTIES 
    OUTPUT_NAME "ndwk"
    POSITION_INDEPENDENT_CODE ON
)

target_include_directories(ndwk_static PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/Inc
    ${CMAKE_CURRENT_SOURCE_DIR}/Lib/sherpa-onnx/include
)

# 共有ライブラリ (libndwk.so) -> Python/他言語 FFI 用
add_library(ndwk_shared SHARED ${CORE_SOURCES})

set_target_properties(ndwk_shared PROPERTIES 
    OUTPUT_NAME "ndwk"
    BUILD_RPATH "${CMAKE_CURRENT_SOURCE_DIR}/Lib/sherpa-onnx/lib"
    INSTALL_RPATH "$ORIGIN/../Lib/sherpa-onnx/lib"
)

target_include_directories(ndwk_shared PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/Inc
    ${CMAKE_CURRENT_SOURCE_DIR}/Lib/sherpa-onnx/include
)

target_link_directories(ndwk_shared PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/Lib/sherpa-onnx/lib
)

target_link_libraries(ndwk_shared PRIVATE
    sherpa-onnx-c-api
    onnxruntime
    m
    pthread
    dl
)

# CLI 実行ファイル
add_executable(ndwk
    Src/main.c
    Src/wav_reader.c
    Src/mic_reader.c
)

target_include_directories(ndwk PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/Inc
    ${CMAKE_CURRENT_SOURCE_DIR}/Lib
    ${CMAKE_CURRENT_SOURCE_DIR}/Lib/sherpa-onnx/include
)

target_link_directories(ndwk PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/Lib/sherpa-onnx/lib
)

target_link_libraries(ndwk PRIVATE
    ndwk_static
    sherpa-onnx-c-api
    onnxruntime
    m
    pthread
    dl
)

set_target_properties(ndwk PROPERTIES
    BUILD_RPATH "${CMAKE_CURRENT_SOURCE_DIR}/Lib/sherpa-onnx/lib"
    INSTALL_RPATH "$ORIGIN/../Lib/sherpa-onnx/lib"
)
