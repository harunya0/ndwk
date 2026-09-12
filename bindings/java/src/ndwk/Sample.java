package ndwk;

import java.io.File;
import java.io.FileInputStream;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;

public class Sample {
    public static void main(String[] args) throws Exception {
        System.out.println("=== ndwk Java (Panama FFM) Demo ===");

        String wavPath = args.length > 0 ? args[0] : "test/ja/ja_033.wav";
        String modelsDir = args.length > 1 ? args[1] : "models";

        if (!new File(wavPath).exists()) {
            System.err.println("WAV file not found: " + wavPath);
            return;
        }

        // 1. WAV 読み込み (16kHz 16-bit Mono -> float[])
        float[] samples = loadWavPcm16(wavPath);
        System.out.printf("Loaded: %s (%.2fs, %d samples)%n", wavPath, samples.length / 16000.0, samples.length);

        // 2. エンジン生成 (try-with-resources で自動解放)
        try (var engine = new NdwkEngine(
                modelsDir,
                NdwkLang.JA,
                true,
                partial -> {
                    System.out.printf("\r\033[32m[Partial(Java)]\033[0m: %s\033[K", partial);
                    System.out.flush();
                },
                (lang, text) -> {
                    System.out.printf("\r\033[33m[Final(Java / %s)]\033[0m: %s\033[K%n", lang, text);
                    System.out.flush();
                }
        )) {
            System.out.println("Streaming audio to ndwk...\n");

            // 3. 疑似ストリーミング (100ms = 1600サンプルずつ投入)
            int chunkSize = 1600;
            for (int i = 0; i < samples.length; i += chunkSize) {
                int len = Math.min(chunkSize, samples.length - i);
                engine.feedAudio(samples, i, len);
                Thread.sleep(50);
            }

            // 4. フラッシュ
            engine.flush();
            Thread.sleep(3000); // 最終結果が upcall で返ってくるまで少し待つ
            System.out.println("\nDone.");
        }
    }

    private static float[] loadWavPcm16(String path) throws Exception {
        try (var fis = new FileInputStream(path)) {
            byte[] header = fis.readNBytes(12);
            if (!"RIFF".equals(new String(header, 0, 4)) || !"WAVE".equals(new String(header, 8, 4))) {
                throw new IllegalArgumentException("Not a valid WAV file");
            }

            byte[] chunkHeader = new byte[8];
            while (fis.read(chunkHeader) == 8) {
                String chunkId = new String(chunkHeader, 0, 4);
                int chunkSize = ByteBuffer.wrap(chunkHeader, 4, 4).order(ByteOrder.LITTLE_ENDIAN).getInt();

                if ("data".equals(chunkId)) {
                    byte[] raw = fis.readNBytes(chunkSize);
                    int sampleCount = chunkSize / 2;
                    float[] samples = new float[sampleCount];
                    ByteBuffer bb = ByteBuffer.wrap(raw).order(ByteOrder.LITTLE_ENDIAN);
                    for (int i = 0; i < sampleCount; i++) {
                        samples[i] = bb.getShort() / 32768.0f;
                    }
                    return samples;
                } else {
                    fis.skipNBytes(chunkSize);
                }
            }
            throw new IllegalArgumentException("Data chunk not found in WAV");
        }
    }
}
