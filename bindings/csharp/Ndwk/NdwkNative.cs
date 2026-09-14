using System;
using System.IO;
using System.Runtime.InteropServices;

namespace Ndwk;
public enum NdwkLang : int
{
    Ja = 0,
    Zh = 1,
    Ko = 2,
    En = 3,
    Omni = 4,
    Count = 5
}

[UnmanagedFunctionPointer(CallingConvention.Cdecl)]
public delegate void NdwkOnPartialCallback(IntPtr textData, IntPtr userData);

[UnmanagedFunctionPointer(CallingConvention.Cdecl)]
public delegate void NdwkOnFinalCallback(NdwkLang lang, IntPtr textData, IntPtr userData);

[StructLayout(LayoutKind.Sequential)]
public struct NdwkConfigNative
{
    public IntPtr ModelsDir;
    public NdwkLang DefaultLang;
    [MarshalAs(UnmanagedType.I1)] public bool AutoDetect;
    [MarshalAs(UnmanagedType.I1)] public bool EnablePunct;

    public float VadThreshold;
    public float VadMinSilenceSec;
    public float VadMinSpeechSec;
    public float VadMaxSpeechSec;

    public int NumThreads;
    public float PartialIntervalSec;
    public float PartialWindowSec;
    public float PrerollSec;

    public IntPtr OnPartial;
    public IntPtr OnFinal;
    public IntPtr UserData;
}

internal static class NdwkNative
{
    private const string LibName = "ndwk";

    static NdwkNative()
    {
        NativeLibrary.SetDllImportResolver(typeof(NdwkNative).Assembly, (libraryName, assembly, searchPath) =>
        {
            if (libraryName != LibName) return IntPtr.Zero;

            // Platform-specific library name
            string libFile = RuntimeInformation.IsOSPlatform(OSPlatform.Windows) ? "ndwk.dll"
                           : RuntimeInformation.IsOSPlatform(OSPlatform.OSX)     ? "libndwk.dylib"
                           :                                                       "libndwk.so";

            string[] candidates = [
                // NuGet runtimes/ (copied to output by MSBuild)
                Path.Combine(AppContext.BaseDirectory, libFile),
                // Local development (build/ relative paths)
                Path.Combine(AppContext.BaseDirectory, "build", libFile),
                Path.Combine(AppContext.BaseDirectory, "..", "build", libFile),
                Path.Combine(AppContext.BaseDirectory, "..", "..", "build", libFile),
                Path.GetFullPath(Path.Combine("build", libFile)),
                Path.GetFullPath(Path.Combine("..", "build", libFile)),
                Path.GetFullPath(Path.Combine("..", "..", "build", libFile)),
            ];

            foreach (var path in candidates)
            {
                if (File.Exists(path) && NativeLibrary.TryLoad(path, out var handle))
                    return handle;
            }
            return IntPtr.Zero;
        });
    }

    [DllImport(LibName, EntryPoint = "ndwk_default_config", CallingConvention = CallingConvention.Cdecl)]
    public static extern NdwkConfigNative DefaultConfig();

    [DllImport(LibName, EntryPoint = "ndwk_create", CallingConvention = CallingConvention.Cdecl)]
    public static extern IntPtr Create(ref NdwkConfigNative config);

    [DllImport(LibName, EntryPoint = "ndwk_destroy", CallingConvention = CallingConvention.Cdecl)]
    public static extern void Destroy(IntPtr engine);

    [DllImport(LibName, EntryPoint = "ndwk_feed_audio", CallingConvention = CallingConvention.Cdecl)]
    public static extern void FeedAudio(IntPtr engine, in float samples, nuint numSamples);

    [DllImport(LibName, EntryPoint = "ndwk_flush", CallingConvention = CallingConvention.Cdecl)]
    public static extern void Flush(IntPtr engine);
}
