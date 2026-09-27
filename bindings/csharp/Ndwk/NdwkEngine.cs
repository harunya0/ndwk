using System;
using System.Runtime.InteropServices;

namespace Ndwk;

public sealed class NdwkEngine : IDisposable
{
    private IntPtr _handle;
    private readonly NdwkOnPartialCallback _partialCallback;
    private readonly NdwkOnFinalCallback _finalCallback;
    private bool _disposed;

    public event Action<string>? OnPartial;
    public event Action<NdwkLang, string>? OnFinal;

    public NdwkEngine(string modelsDir = "models", NdwkLang lang = NdwkLang.Ja, bool enablePunct = true)
        : this(new NdwkConfig { ModelsDir = modelsDir, DefaultLang = lang, EnablePunct = enablePunct })
    {
    }

    public NdwkEngine(NdwkConfig config)
    {
        ArgumentNullException.ThrowIfNull(config);

        var nativeConfig = NdwkNative.DefaultConfig();

        // 設定の上書き
        nativeConfig.ModelsDir = Marshal.StringToCoTaskMemUTF8(config.ModelsDir);
        nativeConfig.DefaultLang = config.DefaultLang;
        nativeConfig.AutoDetect = config.AutoDetect;
        nativeConfig.EnablePunct = config.EnablePunct;
        nativeConfig.VadThreshold = config.VadThreshold;
        nativeConfig.VadMinSilenceSec = config.VadMinSilenceSec;
        nativeConfig.VadMinSpeechSec = config.VadMinSpeechSec;
        nativeConfig.VadMaxSpeechSec = config.VadMaxSpeechSec;
        nativeConfig.NumThreads = config.NumThreads;
        nativeConfig.PartialIntervalSec = config.PartialIntervalSec;
        nativeConfig.PartialWindowSec = config.PartialWindowSec;
        nativeConfig.PrerollSec = config.PrerollSec;

        // GC回収防止のため関数ポインタ化
        _partialCallback = HandlePartial;
        _finalCallback = HandleFinal;
        nativeConfig.OnPartial = Marshal.GetFunctionPointerForDelegate(_partialCallback);
        nativeConfig.OnFinal = Marshal.GetFunctionPointerForDelegate(_finalCallback);

        try
        {
            _handle = NdwkNative.Create(ref nativeConfig);
            if (_handle == IntPtr.Zero)
            {
                throw new InvalidOperationException("Failed to create NdwkEngine instance.");
            }
        }
        finally
        {
            Marshal.FreeCoTaskMem(nativeConfig.ModelsDir);
        }
    }

    private void HandlePartial(IntPtr textPtr, IntPtr userData)
    {
        var text = Marshal.PtrToStringUTF8(textPtr);
        if (text != null) OnPartial?.Invoke(text);
    }

    private void HandleFinal(NdwkLang lang, IntPtr textPtr, IntPtr userData)
    {
        var text = Marshal.PtrToStringUTF8(textPtr);
        if (text != null) OnFinal?.Invoke(lang, text);
    }

    public void FeedAudio(ReadOnlySpan<float> samples)
    {
        ThrowIfDisposed();
        if (samples.IsEmpty) return;

        NdwkNative.FeedAudio(_handle, in MemoryMarshal.GetReference(samples), (nuint)samples.Length);
    }

    public void Flush()
    {
        ThrowIfDisposed();
        NdwkNative.Flush(_handle);
    }

    private void ThrowIfDisposed()
    {
        ObjectDisposedException.ThrowIf(_disposed, this);
    }

    public void Dispose()
    {
        if (!_disposed)
        {
            if (_handle != IntPtr.Zero)
            {
                NdwkNative.Destroy(_handle);
                _handle = IntPtr.Zero;
            }
            _disposed = true;
        }
        GC.SuppressFinalize(this);
    }
    ~NdwkEngine() => Dispose();
}
