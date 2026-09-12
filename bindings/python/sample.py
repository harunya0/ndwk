#!/usr/bin/env python3
"""ndwk Python binding sample — WAV file transcription."""

import sys
import wave
import array
from ndwk import NdwkEngine, NdwkLang

def main() -> None:
    if len(sys.argv) < 3:
        print(f"Usage: {sys.argv[0]} <wav_file> <models_dir>")
        sys.exit(1)

    wav_path = sys.argv[1]
    models_dir = sys.argv[2]

    print("=== ndwk Python (ctypes) Demo ===")

    # WAV読み込み (16kHz mono s16le)
    with wave.open(wav_path, "rb") as wf:
        assert wf.getnchannels() == 1, "Mono WAV required"
        assert wf.getsampwidth() == 2, "16-bit WAV required"
        assert wf.getframerate() == 16000, "16kHz WAV required"
        n_frames = wf.getnframes()
        raw = wf.readframes(n_frames)

    # s16le → float32 [-1.0, 1.0]
    shorts = array.array("h", raw)
    samples = array.array("f", (s / 32768.0 for s in shorts))
    duration = len(samples) / 16000.0
    print(f"Loaded: {wav_path} ({duration:.2f}s, {len(samples)} samples)")

    # コールバック
    def on_partial(text: str) -> None:
        print(f"\r\x1b[32m[Partial]\x1b[0m: {text}\x1b[K", end="")

    def on_final(lang : NdwkLang, text: str) -> None:
        print(f"\r\x1b[32m[Final({lang.name})]\x1b[0m: {text}\x1b[K")

    # エンジン起動
    engine = NdwkEngine(
        models_dir=models_dir,
        lang=NdwkLang.JA,
        on_partial=on_partial,
        on_final=on_final,
    )

    # ストリーミング送信 (1600サンプル = 100ms ずつ)
    print("Streaming audio to ndwk...")
    chunk_size = 1600
    for i in range(0, len(samples), chunk_size):
        engine.feed_audio(samples[i : i + chunk_size])

    # 残りをflush
    engine.flush()
    engine.close()

    print("\nDone.")


if __name__ == "__main__":
    main()
