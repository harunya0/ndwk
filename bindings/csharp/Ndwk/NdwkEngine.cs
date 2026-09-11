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
    {
        // デフォルト情報の取得
        var config = NdwkNative.DefaultConfig();

        // 設定の上書き
        config.ModelsDir = Marshal.StringToHGlobalAnsi(modelsDir);
        config.DefaultLang = lang;
        config.EnablePunct = enablePunct;

        // GC回回収防のため関数ポインタ化
        _partialCallback = HandlePartial;
        _finalCallback = HandleFinal;
        config.OnPartial = Marshal.GetFunctionPointerForDelegate(_partialCallback);
        config.OnFinal = Marshal.GetFunctionPointerForDelegate(_finalCallback);

        try
        {
            _handle = NdwkNative.Create(ref config);
            if (_handle == IntPtr.Zero)
            {
                throw new InvalidOperationException("Failed to create NdwkEngine instance.");
            }
        }
        finally
        {
            Marshal.FreeHGlobal(config.ModelsDir);
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
