namespace Ndwk;

/// <summary>
/// ndwk 音声認識エンジンの詳細設定
/// </summary>
public sealed class NdwkConfig
{
    public string ModelsDir { get; set; } = "models";
    public NdwkLang DefaultLang { get; set; } = NdwkLang.Ja;
    public bool AutoDetect { get; set; }
    public bool EnablePunct { get; set; } = true;
    public float VadThreshold { get; set; }
    public float VadMinSilenceSec { get; set; }
    public float VadMinSpeechSec { get; set; }
    public float VadMaxSpeechSec { get; set; }
    public int NumThreads { get; set; }
    public float PartialIntervalSec { get; set; }
    public float PartialWindowSec { get; set; }
    public float PrerollSec { get; set; }

    public NdwkConfig()
    {
        // ネイティブのデフォルト値（黄金比パラメータ）を反映
        var def = NdwkNative.DefaultConfig();
        DefaultLang = def.DefaultLang;
        AutoDetect = def.AutoDetect;
        EnablePunct = def.EnablePunct;
        VadThreshold = def.VadThreshold;
        VadMinSilenceSec = def.VadMinSilenceSec;
        VadMinSpeechSec = def.VadMinSpeechSec;
        VadMaxSpeechSec = def.VadMaxSpeechSec;
        NumThreads = def.NumThreads;
        PartialIntervalSec = def.PartialIntervalSec;
        PartialWindowSec = def.PartialWindowSec;
        PrerollSec = def.PrerollSec;
    }
}
