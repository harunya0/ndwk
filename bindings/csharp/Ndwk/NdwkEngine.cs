using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using System.Threading;
using System.Threading.Channels;

namespace Ndwk;

public sealed class NdwkEngine : IDisposable
{
    private IntPtr _handle;
    private readonly NdwkOnPartialCallback _partialCallback;
    private readonly NdwkOnFinalCallback _finalCallback;
    private readonly NdwkOnFrameMetaCallback _frameMetaCallback;
    private Channel<NdwkFrameMeta>? _metaChannel;
    private bool _disposed;

    public event Action<string>? OnPartial;
    public event Action<NdwkLang, string>? OnFinal;
    public event Action<NdwkFrameMeta>? OnFrameMeta;

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
        _frameMetaCallback = HandleFrameMeta;
        nativeConfig.OnPartial = Marshal.GetFunctionPointerForDelegate(_partialCallback);
        nativeConfig.OnFinal = Marshal.GetFunctionPointerForDelegate(_finalCallback);
        nativeConfig.OnFrameMeta = Marshal.GetFunctionPointerForDelegate(_frameMetaCallback);

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

    private void HandleFrameMeta(in NdwkFrameMeta meta, IntPtr userData)
    {
        OnFrameMeta?.Invoke(meta);
        _metaChannel?.Writer.TryWrite(meta);
    }

    /// <summary>
    /// 音響フレームメタデータの非同期ストリームを取得します。
    /// </summary>
    public async IAsyncEnumerable<NdwkFrameMeta> GetFrameMetaStreamAsync([System.Runtime.CompilerServices.EnumeratorCancellation] CancellationToken cancellationToken = default)
    {
        ThrowIfDisposed();
        _metaChannel ??= Channel.CreateUnbounded<NdwkFrameMeta>(new UnboundedChannelOptions
        {
            SingleWriter = true
        });

        var reader = _metaChannel.Reader;
        while (!cancellationToken.IsCancellationRequested)
        {
            if (await reader.WaitToReadAsync(cancellationToken).ConfigureAwait(false))
            {
                while (reader.TryRead(out var item))
                {
                    yield return item;
                }
            }
        }
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
            _metaChannel?.Writer.TryComplete();
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
