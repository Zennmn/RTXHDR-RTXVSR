using System.Text.Json.Serialization;

namespace RTXVideoConverter.Core.Models;

public enum ProcessingMode
{
    Vsr,
    Hdr,
    Both,
}

public enum BackendStatus
{
    Starting,
    Ready,
    Degraded,
    Offline,
}

public enum AppTheme
{
    System,
    Light,
    Dark,
}

public sealed record JobHistoryItem(
    string Id,
    string FileName,
    string OutputPath,
    string State,
    DateTimeOffset CompletedAt,
    double AverageFps)
{
    [JsonIgnore]
    public string CompletedAtText => CompletedAt.LocalDateTime.ToString("yyyy-MM-dd HH:mm");
    [JsonIgnore]
    public string AverageFpsText => AverageFps > 0 ? $"{AverageFps:F1} FPS" : "-- FPS";
}
