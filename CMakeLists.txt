cmake_minimum_required(VERSION 3.15)
project(ndwk VERSION 0.1.0 LANGUAGES C)

# C11 標準を強制
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# コンパイル警告をしっかり出す
add_compile_options(-Wall -Wextra -O3 -flto -march=native)
add_link_options(-flto -O3)

# ソースファイルの収集 (Src/ 配下のすべての .c を自動登録)
file(GLOB SOURCES CONFIGURE_DEPENDS "Src/*.c")

# 実行ファイルの定義
add_executable(ndwk ${SOURCES})

# インクルードパスの設定 (ターゲット指定で綺麗に管理)
target_include_directories(ndwk PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/Inc
    ${CMAKE_CURRENT_SOURCE_DIR}/Lib
    ${CMAKE_CURRENT_SOURCE_DIR}/Lib/sherpa-onnx/include
)

# 共有ライブラリ (.so) の探索パス
target_link_directories(ndwk PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/Lib/sherpa-onnx/lib
)

# リンクするライブラリ
# - sherpa-onnx-c-api: 音声認識・VAD本体
# - m: 数学ライブラリ (math.h)
target_link_libraries(ndwk PRIVATE
    sherpa-onnx-c-api
    onnxruntime
    m
    pthread
    dl
)

# RPATH の設定
set_target_properties(ndwk PROPERTIES
    BUILD_RPATH "${CMAKE_CURRENT_SOURCE_DIR}/Lib/sherpa-onnx/lib"
    INSTALL_RPATH "$ORIGIN/../Lib/sherpa-onnx/lib"
)
