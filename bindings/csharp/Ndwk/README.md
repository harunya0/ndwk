# Ndwk

Lightweight, zero-delay, CPU-only real-time multilingual speech recognition (ASR) engine for .NET.

Pure C11 core with idiomatic .NET P/Invoke bindings. Zero external dependencies.

## Supported Languages

- Japanese (ReazonSpeech Zipformer)
- English (NeMo Parakeet TDT v3)
- Chinese (Paraformer-large)
- Korean / Multilingual (SenseVoice Small)
- Automatic Language Identification (Whisper-tiny LID)

## Quick Start

```csharp
using Ndwk;

using var engine = new NdwkEngine(modelsDir: "models", lang: NdwkLang.Ja);

engine.OnPartial += text => Console.Write($"\r[Partial] {text}");
engine.OnFinal += (lang, text) => Console.WriteLine($"\r[{lang}] {text}");

// Feed 16kHz mono float32 audio
engine.FeedAudio(samples);
engine.Flush();
```

## Requirements

- .NET 8.0 or later
- Models must be downloaded separately. See [setup instructions](https://github.com/harunya0/ndwk#quick-start).

## Supported Platforms

| Platform | Runtime Identifier |
|----------|--------------------|
| Linux x64 | `linux-x64` |
| Linux ARM64 | `linux-arm64` |
| macOS ARM64 | `osx-arm64` |
| Windows x64 | `win-x64` |

## License

Apache-2.0
