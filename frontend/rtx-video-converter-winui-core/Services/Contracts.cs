using System.Collections.ObjectModel;
using RTXVideoConverter.Core.Models;

namespace RTXVideoConverter.Core.Services;

public interface IBackendApiClient : IDisposable
{
    Task<HealthResponse> GetHealthAsync(CancellationToken token = default);
    Task<CapabilityResponse> GetCapabilitiesAsync(CancellationToken token = default);
    Task<MediaProbeResponse> ProbeMediaAsync(string inputPath, CancellationToken token = default);
    Task<CreateJobResponse> CreateJobAsync(TranscodeRequest request, CancellationToken token = default);
    Task<JobSnapshot> GetJobAsync(string id, CancellationToken token = default);
    Task CancelJobAsync(string id, CancellationToken token = default);
    Task ShutdownAsync(CancellationToken token = default);
}

public interface IBackendProcessService : IAsyncDisposable
{
    BackendSession Session { get; }
    bool IsRunning { get; }
    bool TryStart(out string? error);
}

public interface IFilePickerService
{
    Task<string?> PickVideoAsync(CancellationToken token = default);
    Task<string?> PickOutputFolderAsync(CancellationToken token = default);
}

public interface IThemeService
{
    AppTheme CurrentTheme { get; }
    bool SystemAnimationsEnabled { get; }
    void ApplyTheme(AppTheme theme);
}

public interface IJobHistoryService
{
    ReadOnlyObservableCollection<JobHistoryItem> Items { get; }
    void Add(JobSnapshot snapshot);
    void Remove(string id);
    void Clear();
}

public interface IOutputLauncherService
{
    bool OpenOutput(string path);
    bool OpenContainingFolder(string path);
}
