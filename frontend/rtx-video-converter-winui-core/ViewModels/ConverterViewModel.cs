using CommunityToolkit.Mvvm.ComponentModel;
using CommunityToolkit.Mvvm.Input;
using RTXVideoConverter.Core.Models;
using RTXVideoConverter.Core.Services;

namespace RTXVideoConverter.Core.ViewModels;

public partial class ConverterViewModel : ObservableObject, IAsyncDisposable
{
    private static readonly HashSet<string> ActiveStates = ["queued", "running", "canceling"];
    private readonly IBackendApiClient _api;
    private readonly IBackendProcessService _backendProcess;
    private readonly IFilePickerService _filePicker;
    private readonly IJobHistoryService _history;
    private readonly CancellationTokenSource _lifetime = new();
    private CancellationTokenSource? _probeCancellation;
    private MediaProbeResponse? _media;
    private CapabilityResponse? _capabilities;
    private string? _activeJobId;
    private Task? _pollingTask;
    private long _probeGeneration;
    private bool _initialized;
    private bool _disposed;

    public ConverterViewModel(
        IBackendApiClient api,
        IBackendProcessService backendProcess,
        IFilePickerService filePicker,
        IJobHistoryService history)
    {
        _api = api;
        _backendProcess = backendProcess;
        _filePicker = filePicker;
        _history = history;
    }

    [ObservableProperty] private BackendStatus backendStatus = BackendStatus.Starting;
    [ObservableProperty] private string backendMessage = "正在启动处理引擎…";
    [ObservableProperty] private string capabilitySummary = "正在检测 RTX 能力";
    [ObservableProperty] private bool isVsrAvailable;
    [ObservableProperty] private bool isHdrAvailable;
    [ObservableProperty] private bool isH264Available;
    [ObservableProperty] private bool isHevcAvailable;
    [ObservableProperty] private bool isAv1Available;

    [ObservableProperty] private string inputPath = string.Empty;
    [ObservableProperty] private string outputDirectory = string.Empty;
    [ObservableProperty] private string outputPath = "未生成";
    [ObservableProperty] private string fileName = "未选择";
    [ObservableProperty] private string fileSize = "-";
    [ObservableProperty] private string resolution = "-";
    [ObservableProperty] private string duration = "-";
    [ObservableProperty] private string sourceCodec = "-";
    [ObservableProperty] private bool isProbing;

    [ObservableProperty] private ProcessingMode selectedMode = ProcessingMode.Both;
    [ObservableProperty] private int selectedQuality = 4;
    [ObservableProperty] private double scaleFactor = 2.0;
    [ObservableProperty] private int contrast = 100;
    [ObservableProperty] private int saturation = 100;
    [ObservableProperty] private int middleGray = 44;
    [ObservableProperty] private int maxLuminance = 1000;
    [ObservableProperty] private int selectedCodecIndex = 1;
    [ObservableProperty] private bool includeAudio = true;
    [ObservableProperty] private bool includeSubtitles = true;

    [ObservableProperty] private bool hasActiveJob;
    [ObservableProperty] private double progressPercent;
    [ObservableProperty] private bool isProgressIndeterminate;
    [ObservableProperty] private string progressText = "0%";
    [ObservableProperty] private string jobStatusText = "状态：等待开始";
    [ObservableProperty] private string primaryActionText = "开始转码";
    [ObservableProperty] private string actionHint = "请选择并成功探测输入视频。";

    [ObservableProperty] private bool hasError;
    [ObservableProperty] private string errorMessage = string.Empty;

    public bool IsVsrMode => SelectedMode == ProcessingMode.Vsr;
    public bool IsHdrMode => SelectedMode == ProcessingMode.Hdr;
    public bool IsBothMode => SelectedMode == ProcessingMode.Both;
    public bool IsBothModeAvailable => IsVsrAvailable && IsHdrAvailable;
    public bool IsQuality1 => SelectedQuality == 1;
    public bool IsQuality2 => SelectedQuality == 2;
    public bool IsQuality3 => SelectedQuality == 3;
    public bool IsQuality4 => SelectedQuality == 4;
    public string ScaleFactorText => $"{ScaleFactor:F1}x";
    public string ScaleFactorSummary
    {
        get
        {
            if (_media is null || !TryParseResolution(_media.Resolution, out var width, out var height))
            {
                return ScaleFactorText;
            }

            return $"{ScaleFactorText} · {EvenScaledDimension(width, ScaleFactor)}×{EvenScaledDimension(height, ScaleFactor)}";
        }
    }
    public string QualityText => $"质量 {SelectedQuality}";
    public IReadOnlyList<int> ContrastOptions { get; } = [50, 75, 100, 125, 150];
    public IReadOnlyList<int> SaturationOptions { get; } = [50, 75, 100, 125, 150];
    public IReadOnlyList<int> MiddleGrayOptions { get; } = [18, 36, 44, 50, 60];
    public IReadOnlyList<int> LuminanceOptions { get; } = [400, 600, 1000, 1500, 2000];
    public bool IsVsrConfigurationEnabled => SelectedMode != ProcessingMode.Hdr && IsVsrAvailable && !HasActiveJob;
    public bool IsHdrConfigurationEnabled => SelectedMode != ProcessingMode.Vsr && IsHdrAvailable && !HasActiveJob;
    public bool IsEditingEnabled => !HasActiveJob;
    public bool IsPrimaryActionEnabled => HasActiveJob || CanStart;
    public bool IsH264CodecOptionEnabled => IsH264Available && SelectedMode == ProcessingMode.Vsr;
    public bool IsHevcCodecOptionEnabled => IsHevcAvailable;
    public bool IsAv1CodecOptionEnabled => IsAv1Available;
    public double VsrCardOpacity => IsVsrConfigurationEnabled ? 1 : 0.52;
    public double HdrCardOpacity => IsHdrConfigurationEnabled ? 1 : 0.52;

    public async Task InitializeAsync()
    {
        if (_initialized || _disposed)
        {
            return;
        }

        _initialized = true;
        if (Environment.GetEnvironmentVariable("RTX_UI_DESIGN_PREVIEW") == "1")
        {
            ApplyCapabilities(new CapabilityResponse(true, true, true, true, true, true, true, []));
            BackendStatus = BackendStatus.Ready;
            BackendMessage = "后端已就绪，可以开始转码任务。";
            if (double.TryParse(Environment.GetEnvironmentVariable("RTX_UI_DESIGN_PROGRESS"), out var previewProgress))
            {
                ProgressPercent = Math.Clamp(previewProgress, 0, 100);
                ProgressText = $"{Math.Round(ProgressPercent)}%";
            }
            RefreshState();
            return;
        }

        BackendStatus = BackendStatus.Starting;
        BackendMessage = "正在启动处理引擎…";
        Exception? lastError = null;

        if (!_backendProcess.TryStart(out var startError) && !string.IsNullOrWhiteSpace(startError))
        {
            lastError = new InvalidOperationException(startError);
        }

        for (var attempt = 0; attempt < 28 && !_lifetime.IsCancellationRequested; attempt++)
        {
            try
            {
                await _api.GetHealthAsync(_lifetime.Token);

                ApplyCapabilities(await _api.GetCapabilitiesAsync(_lifetime.Token));
                BackendMessage = _capabilities?.Messages.Count > 0
                    ? "后端已连接，部分 RTX 能力不可用。"
                    : "后端已就绪，可以开始转码任务。";
                RefreshState();
                return;
            }
            catch (OperationCanceledException) when (_lifetime.IsCancellationRequested)
            {
                return;
            }
            catch (Exception ex)
            {
                lastError = ex;
                if (!_backendProcess.IsRunning && !_backendProcess.TryStart(out var retryError) && !string.IsNullOrWhiteSpace(retryError))
                {
                    lastError = new InvalidOperationException(retryError);
                }
                try
                {
                    await Task.Delay(250, _lifetime.Token);
                }
                catch (OperationCanceledException) when (_lifetime.IsCancellationRequested)
                {
                    return;
                }
            }
        }

        BackendStatus = BackendStatus.Offline;
        CapabilitySummary = "处理引擎离线";
        BackendMessage = lastError?.Message ?? "无法连接处理引擎。";
        ShowError(lastError ?? new InvalidOperationException(BackendMessage));
        RefreshState();
    }

    private void ApplyCapabilities(CapabilityResponse capabilities)
    {
        _capabilities = capabilities;
        IsVsrAvailable = capabilities.VsrAvailable;
        IsHdrAvailable = capabilities.TruehdrAvailable;
        IsH264Available = capabilities.NvencH264Available;
        IsHevcAvailable = capabilities.NvencHevcMain10Available;
        IsAv1Available = capabilities.NvencAv1Available;

        BackendStatus = capabilities.VsrAvailable && capabilities.TruehdrAvailable
            ? BackendStatus.Ready
            : capabilities.VsrAvailable || capabilities.TruehdrAvailable
                ? BackendStatus.Degraded
                : BackendStatus.Offline;

        CapabilitySummary = capabilities.VsrAvailable && capabilities.TruehdrAvailable
            ? "VSR 可用 / HDR 可用"
            : capabilities.VsrAvailable
                ? "VSR 可用 / HDR 不可用"
                : capabilities.TruehdrAvailable
                    ? "VSR 不可用 / HDR 可用"
                    : "RTX 能力不可用";

        if (!IsModeAvailable(SelectedMode))
        {
            SelectedMode = capabilities.VsrAvailable
                ? ProcessingMode.Vsr
                : capabilities.TruehdrAvailable
                    ? ProcessingMode.Hdr
                    : ProcessingMode.Both;
        }

        EnsureCompatibleCodecSelection();

        RefreshState();
    }

    [RelayCommand(CanExecute = nameof(CanEdit))]
    private async Task PickInputAsync()
    {
        var path = await _filePicker.PickVideoAsync(_lifetime.Token);
        if (!string.IsNullOrWhiteSpace(path))
        {
            await LoadInputAsync(path);
        }
    }

    [RelayCommand(CanExecute = nameof(CanEdit))]
    private async Task PickOutputAsync()
    {
        var path = await _filePicker.PickOutputFolderAsync(_lifetime.Token);
        if (!string.IsNullOrWhiteSpace(path))
        {
            OutputDirectory = path;
        }
    }

    public async Task LoadInputAsync(string path)
    {
        if (!CanEdit() || string.IsNullOrWhiteSpace(path))
        {
            return;
        }

        var generation = Interlocked.Increment(ref _probeGeneration);
        var cancellation = CancellationTokenSource.CreateLinkedTokenSource(_lifetime.Token);
        var previousCancellation = Interlocked.Exchange(ref _probeCancellation, cancellation);
        previousCancellation?.Cancel();
        previousCancellation?.Dispose();

        HasError = false;
        InputPath = path;
        FileName = "正在探测…";
        IsProbing = true;
        RefreshState();

        try
        {
            var media = await _api.ProbeMediaAsync(path, cancellation.Token);
            if (generation != Volatile.Read(ref _probeGeneration))
            {
                return;
            }

            _media = media;
            InputPath = media.Path;
            FileName = media.Name;
            FileSize = FormatBytes(media.SizeBytes);
            Resolution = ValueOrDash(media.Resolution);
            Duration = ValueOrDash(media.Duration);
            SourceCodec = ValueOrDash(media.Codec);

            OutputDirectory = Path.GetDirectoryName(media.Path) ?? Path.GetDirectoryName(path) ?? string.Empty;

            UpdateOutputPath();
            OnPropertyChanged(nameof(ScaleFactorSummary));
        }
        catch (OperationCanceledException) when (cancellation.IsCancellationRequested)
        {
            return;
        }
        catch (Exception ex)
        {
            if (generation != Volatile.Read(ref _probeGeneration))
            {
                return;
            }

            _media = null;
            FileName = "探测失败";
            FileSize = Resolution = Duration = SourceCodec = "-";
            OutputPath = "未生成";
            ShowError(ex);
        }
        finally
        {
            if (generation == Volatile.Read(ref _probeGeneration))
            {
                IsProbing = false;
                RefreshState();
            }
        }
    }

    [RelayCommand]
    private void SelectVsrMode() => SelectMode(ProcessingMode.Vsr);

    [RelayCommand]
    private void SelectHdrMode() => SelectMode(ProcessingMode.Hdr);

    [RelayCommand]
    private void SelectBothMode() => SelectMode(ProcessingMode.Both);

    private void SelectMode(ProcessingMode mode)
    {
        if (SelectedMode != mode)
        {
            SelectedMode = mode;
            return;
        }

        // ToggleButton toggles before executing its command. Re-raise the derived
        // checked states when the active segment is clicked so it cannot remain off.
        OnPropertyChanged(nameof(IsVsrMode));
        OnPropertyChanged(nameof(IsHdrMode));
        OnPropertyChanged(nameof(IsBothMode));
    }

    [RelayCommand]
    private void SelectQuality1() => SelectedQuality = 1;

    [RelayCommand]
    private void SelectQuality2() => SelectedQuality = 2;

    [RelayCommand]
    private void SelectQuality3() => SelectedQuality = 3;

    [RelayCommand]
    private void SelectQuality4() => SelectedQuality = 4;

    [RelayCommand(CanExecute = nameof(CanExecutePrimary))]
    private async Task PrimaryAsync()
    {
        if (HasActiveJob)
        {
            await CancelActiveJobAsync();
            return;
        }

        if (!CanStart || _media is null)
        {
            ShowError(new InvalidOperationException(ActionHint));
            return;
        }

        HasError = false;
        PrimaryActionText = "提交中…";
        try
        {
            var request = BuildRequest();
            var created = await _api.CreateJobAsync(request, _lifetime.Token);
            _activeJobId = created.Id;
            HasActiveJob = true;
            IsProgressIndeterminate = true;
            PrimaryActionText = "取消任务";
            ActionHint = "转换期间参数已锁定。";
            JobStatusText = "状态：排队中 · 正在校验任务";
            _pollingTask = PollJobAsync(created.Id);
        }
        catch (Exception ex)
        {
            _activeJobId = null;
            HasActiveJob = false;
            PrimaryActionText = "开始转码";
            ShowError(ex);
        }
        finally
        {
            RefreshState();
        }
    }

    private async Task PollJobAsync(string id)
    {
        try
        {
            while (!_lifetime.IsCancellationRequested && string.Equals(_activeJobId, id, StringComparison.Ordinal))
            {
                await Task.Delay(500, _lifetime.Token);
                var job = await _api.GetJobAsync(id, _lifetime.Token);
                ApplyJob(job);

                if (!ActiveStates.Contains(job.State))
                {
                    _history.Add(job);
                    _activeJobId = null;
                    HasActiveJob = false;
                    IsProgressIndeterminate = false;
                    PrimaryActionText = "开始转码";
                    ActionHint = job.State == "succeeded" ? "转换已完成，可以再次开始。" : "任务已结束，请检查状态信息。";
                    if (job.Error is not null)
                    {
                        ShowError(new InvalidOperationException($"{job.Error.Message}\n{job.Error.Details}"));
                    }
                    RefreshState();
                    return;
                }
            }
        }
        catch (OperationCanceledException) when (_lifetime.IsCancellationRequested)
        {
        }
        catch (Exception ex)
        {
            _activeJobId = null;
            HasActiveJob = false;
            IsProgressIndeterminate = false;
            PrimaryActionText = "开始转码";
            ShowError(ex);
            RefreshState();
        }
    }

    private void ApplyJob(JobSnapshot job)
    {
        ProgressPercent = Math.Clamp(job.Progress * 100, 0, 100);
        ProgressText = $"{Math.Round(ProgressPercent)}%";
        IsProgressIndeterminate = job.Progress <= 0 && ActiveStates.Contains(job.State);
        JobStatusText = $"状态：{TranslateState(job.State)} · {TranslateStage(job.Stage)} · {job.Fps:F1} FPS · {FormatEta(job.EtaSeconds)}";
    }

    private async Task CancelActiveJobAsync()
    {
        if (_activeJobId is null)
        {
            return;
        }

        try
        {
            PrimaryActionText = "正在取消…";
            await _api.CancelJobAsync(_activeJobId, _lifetime.Token);
        }
        catch (Exception ex)
        {
            PrimaryActionText = "取消任务";
            ShowError(ex);
        }
    }

    private TranscodeRequest BuildRequest()
    {
        var codec = SelectedCodecIndex switch
        {
            0 => "h264",
            2 => "av1",
            _ => "hevc",
        };
        return new TranscodeRequest(
            _media!.Path,
            OutputPath,
            new ProcessingOptions(
                new VsrOptions(SelectedMode is ProcessingMode.Vsr or ProcessingMode.Both, SelectedQuality, ScaleFactor),
                new HdrOptions(
                    SelectedMode is ProcessingMode.Hdr or ProcessingMode.Both,
                    Contrast,
                    Saturation,
                    MiddleGray,
                    MaxLuminance)),
            new OutputOptions(
                "mp4",
                codec,
                IncludeAudio ? "copy" : "none",
                IncludeSubtitles ? "copy-compatible" : "none"));
    }

    partial void OnSelectedModeChanged(ProcessingMode value)
    {
        EnsureCompatibleCodecSelection();

        OnPropertyChanged(nameof(IsVsrMode));
        OnPropertyChanged(nameof(IsHdrMode));
        OnPropertyChanged(nameof(IsBothMode));
        NotifyCodecOptionStateChanged();
        RefreshState();
    }

    partial void OnSelectedCodecIndexChanged(int value) => RefreshState();

    partial void OnSelectedQualityChanged(int value)
    {
        OnPropertyChanged(nameof(IsQuality1));
        OnPropertyChanged(nameof(IsQuality2));
        OnPropertyChanged(nameof(IsQuality3));
        OnPropertyChanged(nameof(IsQuality4));
        OnPropertyChanged(nameof(QualityText));
    }

    partial void OnScaleFactorChanged(double value)
    {
        OnPropertyChanged(nameof(ScaleFactorText));
        OnPropertyChanged(nameof(ScaleFactorSummary));
    }

    partial void OnOutputDirectoryChanged(string value) => UpdateOutputPath();
    partial void OnIsVsrAvailableChanged(bool value) => RefreshState();
    partial void OnIsHdrAvailableChanged(bool value) => RefreshState();
    partial void OnIsH264AvailableChanged(bool value) => NotifyCodecOptionStateChanged();
    partial void OnIsHevcAvailableChanged(bool value) => NotifyCodecOptionStateChanged();
    partial void OnIsAv1AvailableChanged(bool value) => NotifyCodecOptionStateChanged();
    partial void OnHasActiveJobChanged(bool value) => RefreshState();
    partial void OnIsProbingChanged(bool value) => RefreshState();

    private bool CanEdit() => !HasActiveJob && !_disposed;
    private bool CanExecutePrimary() => !_disposed && IsPrimaryActionEnabled;
    private bool CanStart => _media is not null && _capabilities is not null && !IsProbing && !HasActiveJob &&
        IsModeAvailable(SelectedMode) && IsSelectedCodecAvailable();

    private bool IsModeAvailable(ProcessingMode mode) => mode switch
    {
        ProcessingMode.Vsr => IsVsrAvailable,
        ProcessingMode.Hdr => IsHdrAvailable,
        _ => IsVsrAvailable && IsHdrAvailable,
    };

    private bool IsSelectedCodecAvailable() => SelectedCodecIndex switch
    {
        0 => IsH264CodecOptionEnabled,
        1 => IsHevcCodecOptionEnabled,
        2 => IsAv1CodecOptionEnabled,
        _ => false,
    };

    private void EnsureCompatibleCodecSelection()
    {
        if (IsSelectedCodecAvailable())
        {
            return;
        }

        SelectedCodecIndex = IsHevcCodecOptionEnabled
            ? 1
            : IsAv1CodecOptionEnabled
                ? 2
                : IsH264CodecOptionEnabled
                    ? 0
                    : -1;
    }

    private void NotifyCodecOptionStateChanged()
    {
        OnPropertyChanged(nameof(IsH264CodecOptionEnabled));
        OnPropertyChanged(nameof(IsHevcCodecOptionEnabled));
        OnPropertyChanged(nameof(IsAv1CodecOptionEnabled));
        RefreshState();
    }

    private void RefreshState()
    {
        OnPropertyChanged(nameof(IsBothModeAvailable));
        OnPropertyChanged(nameof(IsEditingEnabled));
        OnPropertyChanged(nameof(IsVsrConfigurationEnabled));
        OnPropertyChanged(nameof(IsHdrConfigurationEnabled));
        OnPropertyChanged(nameof(VsrCardOpacity));
        OnPropertyChanged(nameof(HdrCardOpacity));
        OnPropertyChanged(nameof(IsPrimaryActionEnabled));

        if (!HasActiveJob)
        {
            PrimaryActionText = "开始转码";
            ActionHint = _capabilities is null
                ? "正在等待处理引擎。"
                : _media is null
                    ? "请选择并成功探测输入视频。"
                    : !IsModeAvailable(SelectedMode)
                        ? "当前处理模式不可用。"
                        : !IsSelectedCodecAvailable()
                            ? "当前输出编码不可用。"
                        : "所有设置已就绪。";
        }

        PickInputCommand.NotifyCanExecuteChanged();
        PickOutputCommand.NotifyCanExecuteChanged();
        PrimaryCommand.NotifyCanExecuteChanged();
    }

    private void UpdateOutputPath()
    {
        if (_media is null || string.IsNullOrWhiteSpace(OutputDirectory))
        {
            OutputPath = "未生成";
            return;
        }

        try
        {
            OutputPath = Path.Combine(OutputDirectory.Trim(), $"{Path.GetFileNameWithoutExtension(_media.Name)} RTX Converter.mp4");
        }
        catch (Exception ex) when (ex is ArgumentException or NotSupportedException)
        {
            OutputPath = "输出目录无效";
        }
    }

    [RelayCommand]
    private void DismissError() => HasError = false;

    private void ShowError(Exception exception)
    {
        ErrorMessage = exception is BackendApiException api && !string.IsNullOrWhiteSpace(api.Details)
            ? $"{api.Message}  {api.Details}"
            : exception.Message;
        HasError = true;
    }

    public async ValueTask DisposeAsync()
    {
        if (_disposed)
        {
            return;
        }

        _disposed = true;
        _probeCancellation?.Cancel();
        _lifetime.Cancel();

        try
        {
            using var timeout = new CancellationTokenSource(TimeSpan.FromMilliseconds(1000));
            await _api.ShutdownAsync(timeout.Token);
        }
        catch
        {
            // The process service below is the guaranteed local fallback.
        }

        if (_pollingTask is not null)
        {
            try
            {
                await _pollingTask;
            }
            catch (OperationCanceledException)
            {
            }
        }

        _api.Dispose();
        await _backendProcess.DisposeAsync();
        _probeCancellation?.Dispose();
        _lifetime.Dispose();
    }

    private static string ValueOrDash(string value) => string.IsNullOrWhiteSpace(value) ? "-" : value;
    private static bool TryParseResolution(string value, out int width, out int height)
    {
        width = 0;
        height = 0;
        if (string.IsNullOrWhiteSpace(value))
        {
            return false;
        }

        var parts = value.Split(['x', 'X', '×'], StringSplitOptions.TrimEntries | StringSplitOptions.RemoveEmptyEntries);
        return parts.Length == 2 &&
            int.TryParse(parts[0], out width) && width > 0 &&
            int.TryParse(parts[1], out height) && height > 0;
    }

    private static int EvenScaledDimension(int dimension, double scale)
    {
        var rounded = Math.Max(2L, (long)Math.Round(dimension * scale, MidpointRounding.AwayFromZero));
        if ((rounded & 1) != 0)
        {
            rounded++;
        }

        return (int)Math.Min(rounded, int.MaxValue);
    }
    private static string FormatBytes(long bytes) => bytes <= 0
        ? "-"
        : bytes >= 1L << 30
            ? $"{bytes / (double)(1L << 30):F2} GB"
            : $"{bytes / (double)(1L << 20):F1} MB";
    private static string FormatEta(long seconds) => seconds <= 0 ? "ETA --" : $"ETA {TimeSpan.FromSeconds(seconds):mm\\:ss}";
    private static string TranslateState(string state) => state switch
    {
        "queued" => "排队中",
        "running" => "处理中",
        "canceling" => "正在取消",
        "succeeded" => "已完成",
        "failed" => "失败",
        "canceled" => "已取消",
        _ => state,
    };
    private static string TranslateStage(string stage) => stage switch
    {
        "validating" => "校验",
        "probing" => "探测媒体",
        "initializing_gpu" => "初始化 GPU",
        "decoding" => "解码",
        "processing_rtx" => "RTX 增强",
        "encoding" => "编码",
        "muxing" => "封装",
        "finalizing" => "完成处理",
        _ => stage,
    };
}
