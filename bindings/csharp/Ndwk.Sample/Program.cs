using System;
using System.IO;
using System.Threading;
using Ndwk;

Console.WriteLine("=== Ndwk C# (.NET 10) Sample ===");

var wavPath = args.Length > 0 ? args[0] : "test/ja/ja_033.wav";
var modelsDir = args.Length > 1 ? args[1] : "models";

if (!File.Exists(wavPath))
{
    Console.WriteLine($"WAV file not found: {wavPath}");
    return;
}

// wav読み込み
var audioSamples = LoadWavPcm16(wavPath);
Console.WriteLine($"Loaded: {wavPath} ({audioSamples.Length / 16000.0:F2}s, {audioSamples.Length} samples)");

// エンジン生成
using var engine = new NdwkEngine(modelsDir: modelsDir, lang: NdwkLang.Ja);

// イベント購買
engine.OnPartial += text =>
{
    Console.Write($"\r\x1b[32m[Partial]\x1b[0m: {text}\x1b[K");
};

engine.OnFinal += (lang, text) =>
{
    Console.Write($"\r\x1b[33m[Final]\x1b[0m ({lang}) {text}\x1b[K");
};

Console.WriteLine("=== Feeding audio samples ===");
// 音声サンプルを FeedAudio で渡す
int chunkSize = 1600;
for (int i = 0; i < audioSamples.Length; i += chunkSize)
{
    int length = Math.Min(chunkSize, audioSamples.Length - i);
    engine.FeedAudio(audioSamples.AsSpan(i, length));
    Thread.Sleep(50); // 疑似ストリーミング
}

// 終端フラッシュ
engine.Flush();
Console.WriteLine("=== Done ===");

static float[] LoadWavPcm16(string path)
{
    using var fs = new FileStream(path, FileMode.Open, FileAccess.Read);
    using var br = new BinaryReader(fs);
    
    // RIFF ヘッダ
    var riff = new string(br.ReadChars(4));
    br.ReadInt32(); // ファイルサイズ
    var wave = new string(br.ReadChars(4));
    if (riff != "RIFF" || wave != "WAVE") throw new InvalidDataException("Invalid WAV file.");

    while (fs.Position < fs.Length)
    {
        var chunkId = new string(br.ReadChars(4));
        var chunkSize = br.ReadInt32();
        if (chunkId == "data")
        {
            int sampleCount = chunkSize / 2; // PCM16 は 2 バイト/sample
            var samples = new float[sampleCount];
            for (int i = 0; i < sampleCount; i++)
            {
                samples[i] = br.ReadInt16() / 32768.0f; // PCM16 -> float [-1, 1]
            }
            return samples;
        }
        fs.Position += chunkSize; // 他のチャンクはスキップ
    }
    throw new InvalidDataException("No data chunk found in WAV file.");
}
