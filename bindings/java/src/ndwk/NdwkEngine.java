package ndwk;

import java.lang.foreign.*;
import java.lang.invoke.MethodHandle;
import java.lang.invoke.MethodHandles;
import java.lang.invoke.MethodType;
import java.nio.file.Path;
import java.util.function.BiConsumer;
import java.util.function.Consumer;

public final class NdwkEngine implements AutoCloseable {
    public static final StructLayout CONFIG_LAYOUT = MemoryLayout.structLayout(
        ValueLayout.ADDRESS.withName("models_dir"),
        ValueLayout.JAVA_INT.withName("default_lang"),
        ValueLayout.JAVA_BOOLEAN.withName("auto_detect"),
        ValueLayout.JAVA_BOOLEAN.withName("enable_punct"),
        MemoryLayout.paddingLayout(2), // 2バイトパディング (floatのアライメント調整)
        ValueLayout.JAVA_FLOAT.withName("vad_threshold"),
        ValueLayout.JAVA_FLOAT.withName("vad_min_silence_sec"),
        ValueLayout.JAVA_FLOAT.withName("vad_min_speech_sec"),
        ValueLayout.JAVA_FLOAT.withName("vad_max_speech_sec"),
        ValueLayout.JAVA_INT.withName("num_threads"),
        ValueLayout.JAVA_FLOAT.withName("partial_interval_sec"),
        ValueLayout.JAVA_FLOAT.withName("partial_window_sec"),
        ValueLayout.JAVA_FLOAT.withName("preroll_sec"),
        ValueLayout.ADDRESS.withName("on_partial"),
        ValueLayout.ADDRESS.withName("on_final"),
        ValueLayout.ADDRESS.withName("user_data")
    );

    private final Arena arena;
    private final MemorySegment handle;
    private final MethodHandle feedAudioMH;
    private final MethodHandle flushMH;
    private final MethodHandle destroyMH;
    private boolean closed = false;

    private final Consumer<String> onPartial;
    private final BiConsumer<NdwkLang, String> onFinal;

    @SuppressWarnings("this-escape")
    public NdwkEngine(String modelsDir, NdwkLang lang, boolean enablePunct,
                      Consumer<String> onPartial, BiConsumer<NdwkLang, String> onFinal) {
        this.onPartial = onPartial;
        this.onFinal = onFinal;
        this.arena = Arena.ofShared();

        try {
            Linker linker = Linker.nativeLinker();
            
            // libndwk.so をロード (プロジェクトルートまたは相対パス)
            Path libPath = findLibrary();
            SymbolLookup lookup = SymbolLookup.libraryLookup(libPath, arena);
            // C関数ハンドル取得
            MethodHandle defaultCfgMH = linker.downcallHandle(
                lookup.findOrThrow("ndwk_default_config"),
                FunctionDescriptor.of(CONFIG_LAYOUT)
            );
            MethodHandle createMH = linker.downcallHandle(
                lookup.findOrThrow("ndwk_create"),
                FunctionDescriptor.of(ValueLayout.ADDRESS, ValueLayout.ADDRESS)
            );
            this.destroyMH = linker.downcallHandle(
                lookup.findOrThrow("ndwk_destroy"),
                FunctionDescriptor.ofVoid(ValueLayout.ADDRESS)
            );
            this.feedAudioMH = linker.downcallHandle(
                lookup.findOrThrow("ndwk_feed_audio"),
                FunctionDescriptor.ofVoid(ValueLayout.ADDRESS, ValueLayout.ADDRESS, ValueLayout.JAVA_LONG)
            );
            this.flushMH = linker.downcallHandle(
                lookup.findOrThrow("ndwk_flush"),
                FunctionDescriptor.ofVoid(ValueLayout.ADDRESS)
            );
            // 1. デフォルト設定を取得
            MemorySegment config = (MemorySegment) defaultCfgMH.invokeExact((SegmentAllocator) arena);
            // 2. コールバック UpcallStub の作成
            MethodHandle partialTarget = MethodHandles.lookup().bind(this, "handlePartial",
                MethodType.methodType(void.class, MemorySegment.class, MemorySegment.class));
            MemorySegment partialStub = linker.upcallStub(
                partialTarget,
                FunctionDescriptor.ofVoid(ValueLayout.ADDRESS, ValueLayout.ADDRESS),
                arena
            );
            MethodHandle finalTarget = MethodHandles.lookup().bind(this, "handleFinal",
                MethodType.methodType(void.class, int.class, MemorySegment.class, MemorySegment.class));
            MemorySegment finalStub = linker.upcallStub(
                finalTarget,
                FunctionDescriptor.ofVoid(ValueLayout.JAVA_INT, ValueLayout.ADDRESS, ValueLayout.ADDRESS),
                arena
            );
            // 3. 設定値を上書き
            MemorySegment cModelsDir = arena.allocateFrom(modelsDir);
            config.set(ValueLayout.ADDRESS, CONFIG_LAYOUT.byteOffset(MemoryLayout.PathElement.groupElement("models_dir")), cModelsDir);
            config.set(ValueLayout.JAVA_INT, CONFIG_LAYOUT.byteOffset(MemoryLayout.PathElement.groupElement("default_lang")), lang.getValue());
            config.set(ValueLayout.JAVA_BOOLEAN, CONFIG_LAYOUT.byteOffset(MemoryLayout.PathElement.groupElement("enable_punct")), enablePunct);
            config.set(ValueLayout.ADDRESS, CONFIG_LAYOUT.byteOffset(MemoryLayout.PathElement.groupElement("on_partial")), partialStub);
            config.set(ValueLayout.ADDRESS, CONFIG_LAYOUT.byteOffset(MemoryLayout.PathElement.groupElement("on_final")), finalStub);
            config.set(ValueLayout.JAVA_BOOLEAN, CONFIG_LAYOUT.byteOffset(MemoryLayout.PathElement.groupElement("auto_detect")), false);

            // 4. エンジン生成
            this.handle = (MemorySegment) createMH.invokeExact(config);
            if (handle.equals(MemorySegment.NULL)) {
                throw new IllegalStateException("Failed to create ndwk engine.");
            }
        } catch (Throwable t) {
            arena.close();
            throw new RuntimeException("Engine initialization failed", t);
        }
    }

   void handlePartial(MemorySegment text, MemorySegment userData) {
        if (!text.equals(MemorySegment.NULL) && onPartial != null) {
            onPartial.accept(text.reinterpret(Long.MAX_VALUE).getString(0));
        }
    }

    void handleFinal(int langVal, MemorySegment text, MemorySegment userData) {
        if (!text.equals(MemorySegment.NULL) && onFinal != null) {
            onFinal.accept(NdwkLang.fromValue(langVal), text.reinterpret(Long.MAX_VALUE).getString(0));
        }
    } 

    private MemorySegment nativeAudioBuffer;

    public void feedAudio(float[] samples, int offset, int length) {
        if (closed || length <= 0) return;
        try {
            long requiredBytes = (long) length * Float.BYTES;
            if (nativeAudioBuffer == null || nativeAudioBuffer.byteSize() < requiredBytes) {
                nativeAudioBuffer = arena.allocate(ValueLayout.JAVA_FLOAT, Math.max(length, 16000));
            }
            MemorySegment.copy(samples, offset, nativeAudioBuffer, ValueLayout.JAVA_FLOAT, 0, length);
            feedAudioMH.invokeExact(handle, nativeAudioBuffer, (long) length);
        } catch (Throwable t) {
            throw new RuntimeException(t);
        }
    }

    public void flush() {
        if (closed) return;
        try {
            flushMH.invokeExact(handle);
        } catch (Throwable t) {
            throw new RuntimeException(t);
        }
    }

    @Override
    public void close() {
        if (closed) return;
        closed = true;
        try (arena) {
            if (!handle.equals(MemorySegment.NULL)) {
                destroyMH.invokeExact(handle);
            }
        } catch (Throwable ignored) {
        }
    }

    private static Path findLibrary() {
        String[] candidates = {
            "build/libndwk.so",
            "../../build/libndwk.so",
            "../../../build/libndwk.so",
            "libndwk.so"
        };
        for (String c : candidates) {
            Path p = Path.of(c).toAbsolutePath().normalize();
            if (java.nio.file.Files.exists(p)) return p;
        }
        return Path.of("build/libndwk.so");
    }
}
