# ndwk

Lightweight, zero-delay, CPU-only real-time multilingual speech recognition (ASR) engine for Rust.

Pure C11 core with safe Rust FFI bindings. Prebuilt native libraries are automatically downloaded during build.

## Supported Languages

- Japanese (ReazonSpeech Zipformer)
- English (NeMo Parakeet TDT v3)
- Chinese (Paraformer-large)
- Korean / Multilingual (SenseVoice Small)
- Automatic Language Identification (Whisper-tiny LID)

## Quick Start

```toml
[dependencies]
ndwk = "0.1"
```

```rust
use ndwk::{NdwkEngine, NdwkLang};

let mut engine = NdwkEngine::new(
    "models",
    NdwkLang::Ja,
    true,
    |text| print!("\r[Partial] {text}"),
    |lang, text| println!("\r[{lang:?}] {text}"),
)?;

// Feed 16kHz mono f32 audio
engine.feed_audio(&samples);
engine.flush();
```

## Requirements

- Models must be downloaded separately. See [setup instructions](https://github.com/harunya0/ndwk#quick-start).

## Local Development

To use locally built libraries instead of downloading:

```bash
export NDWK_LIB_DIR=/path/to/ndwk/build
cargo build
```

## Supported Platforms

| Platform | Target Triple |
|----------|---------------|
| Linux x64 | `x86_64-unknown-linux-gnu` |
| Linux ARM64 | `aarch64-unknown-linux-gnu` |
| macOS ARM64 | `aarch64-apple-darwin` |
| Windows x64 | `x86_64-pc-windows-msvc` |

## License

Apache-2.0
