using RTXVideoConverter.Core.Models;
using RTXVideoConverter.Core.Services;
using RTXVideoConverter.Core.ViewModels;
using Xunit;

namespace RTXVideoConverter.Core.Tests;

public sealed class ConverterViewModelTests : IDisposable
{
    private readonly string _directory = Path.Combine(Path.GetTempPath(), $"rtx-core-tests-{Guid.NewGuid():N}");

    [Fact]
    public async Task PrimaryActionRequiresSuccessfullyProbedMedia()
    {
        var api = new FakeBackendApiClient();
        await using var process = new FakeBackendProcessService();
        await using var viewModel = CreateViewModel(api, process);

        await viewModel.InitializeAsync();
        Assert.False(viewModel.IsPrimaryActionEnabled);

        await viewModel.LoadInputAsync("input.mp4");
        Assert.True(viewModel.IsPrimaryActionEnabled);
    }

    [Fact]
    public void LuminanceOptionsStayWithinBackendContract()
    {
        var api = new FakeBackendApiClient();
        var process = new FakeBackendProcessService();
        var viewModel = CreateViewModel(api, process);

        Assert.All(viewModel.LuminanceOptions, value => Assert.InRange(value, 400, 2000));
    }

    [Fact]
    public async Task LatestProbeWinsWhenAnOlderProbeCompletesLate()
    {
        var api = new FakeBackendApiClient();
        await using var process = new FakeBackendProcessService();
        await using var viewModel = CreateViewModel(api, process);

        var firstProbe = viewModel.LoadInputAsync("first.mp4");
        await api.FirstProbeStarted.Task;
        var secondProbe = viewModel.LoadInputAsync("second.mp4");
        await secondProbe;

        api.CompleteFirstProbe();
        await firstProbe;

        Assert.Equal("second.mp4", viewModel.FileName);
        Assert.Equal("second.mp4", viewModel.InputPath);
        Assert.False(viewModel.IsProbing);
        Assert.False(viewModel.HasError);
    }

    [Fact]
    public async Task InputDefaultsOutputBesideSourceWithConverterSuffix()
    {
        var api = new FakeBackendApiClient();
        await using var process = new FakeBackendProcessService();
        await using var viewModel = CreateViewModel(api, process);
        var input = Path.Combine(_directory, "sample clip.mkv");

        await viewModel.LoadInputAsync(input);

        Assert.Equal(_directory, viewModel.OutputDirectory);
        Assert.Equal(Path.Combine(_directory, "sample clip RTX Converter.mp4"), viewModel.OutputPath);
    }

    [Fact]
    public async Task ScaleFactorSummaryIncludesLiveOutputResolution()
    {
        var api = new FakeBackendApiClient();
        await using var process = new FakeBackendProcessService();
        await using var viewModel = CreateViewModel(api, process);

        await viewModel.LoadInputAsync(Path.Combine(_directory, "input.mp4"));
        Assert.Equal("2.0x · 3840×2160", viewModel.ScaleFactorSummary);

        viewModel.ScaleFactor = 1.5;
        Assert.Equal("1.5x · 2880×1620", viewModel.ScaleFactorSummary);
    }

    [Fact]
    public async Task Av1SelectionIsSentAsOutputCodec()
    {
        var api = new FakeBackendApiClient();
        await using var process = new FakeBackendProcessService();
        await using var viewModel = CreateViewModel(api, process);

        await viewModel.InitializeAsync();
        await viewModel.LoadInputAsync(Path.Combine(_directory, "input.mp4"));
        viewModel.SelectedCodecIndex = 2;

        await viewModel.PrimaryCommand.ExecuteAsync(null);

        Assert.NotNull(api.LastRequest);
        Assert.Equal("av1", api.LastRequest.Output.VideoCodec);
    }

    [Fact]
    public async Task HdrModeAllowsAv1ButCoercesH264ToHevc()
    {
        var api = new FakeBackendApiClient();
        await using var process = new FakeBackendProcessService();
        await using var viewModel = CreateViewModel(api, process);

        await viewModel.InitializeAsync();
        viewModel.SelectedMode = ProcessingMode.Vsr;
        viewModel.SelectedCodecIndex = 2;
        viewModel.SelectedMode = ProcessingMode.Both;
        Assert.Equal(2, viewModel.SelectedCodecIndex);

        viewModel.SelectedMode = ProcessingMode.Vsr;
        viewModel.SelectedCodecIndex = 0;
        viewModel.SelectedMode = ProcessingMode.Hdr;
        Assert.Equal(1, viewModel.SelectedCodecIndex);
        Assert.False(viewModel.IsH264CodecOptionEnabled);
        Assert.True(viewModel.IsAv1CodecOptionEnabled);
    }

    private ConverterViewModel CreateViewModel(IBackendApiClient api, IBackendProcessService process)
    {
        Directory.CreateDirectory(_directory);
        return new ConverterViewModel(
            api,
            process,
            new FakeFilePickerService(),
            new JobHistoryService(Path.Combine(_directory, "history.json")));
    }

    public void Dispose()
    {
        if (Directory.Exists(_directory))
        {
            Directory.Delete(_directory, recursive: true);
        }
    }

    private sealed class FakeBackendApiClient : IBackendApiClient
    {
        private readonly TaskCompletionSource<MediaProbeResponse> _firstProbe = new(TaskCreationOptions.RunContinuationsAsynchronously);
        public TaskCompletionSource FirstProbeStarted { get; } = new(TaskCreationOptions.RunContinuationsAsynchronously);
        public TranscodeRequest? LastRequest { get; private set; }

        public Task<HealthResponse> GetHealthAsync(CancellationToken token = default) =>
            Task.FromResult(new HealthResponse("test", true));

        public Task<CapabilityResponse> GetCapabilitiesAsync(CancellationToken token = default) =>
            Task.FromResult(new CapabilityResponse(true, true, true, true, true, true, true, []));

        public Task<MediaProbeResponse> ProbeMediaAsync(string inputPath, CancellationToken token = default)
        {
            if (inputPath == "first.mp4")
            {
                FirstProbeStarted.TrySetResult();
                return _firstProbe.Task;
            }

            return Task.FromResult(Media(inputPath));
        }

        public void CompleteFirstProbe() => _firstProbe.TrySetResult(Media("first.mp4"));
        public Task<CreateJobResponse> CreateJobAsync(TranscodeRequest request, CancellationToken token = default)
        {
            LastRequest = request;
            return Task.FromResult(new CreateJobResponse("job-1"));
        }
        public Task<JobSnapshot> GetJobAsync(string id, CancellationToken token = default) => throw new NotSupportedException();
        public Task CancelJobAsync(string id, CancellationToken token = default) => Task.CompletedTask;
        public Task ShutdownAsync(CancellationToken token = default) => Task.CompletedTask;
        public void Dispose() { }

        private static MediaProbeResponse Media(string path) =>
            new(path, Path.GetFileName(path), 1024, "1920x1080", "00:00:01", "h264 8-bit", []);
    }

    private sealed class FakeBackendProcessService : IBackendProcessService
    {
        public BackendSession Session { get; } = new(49321);
        public bool IsRunning => true;
        public bool TryStart(out string? error) { error = null; return true; }
        public ValueTask DisposeAsync() => ValueTask.CompletedTask;
    }

    private sealed class FakeFilePickerService : IFilePickerService
    {
        public Task<string?> PickVideoAsync(CancellationToken token = default) => Task.FromResult<string?>(null);
        public Task<string?> PickOutputFolderAsync(CancellationToken token = default) => Task.FromResult<string?>(null);
    }
}
