using RTXVideoConverter.Core.Models;
using RTXVideoConverter.Core.Services;
using RTXVideoConverter.Core.ViewModels;
using Xunit;

namespace RTXVideoConverter.Core.Tests;

public sealed class JobHistoryServiceTests : IDisposable
{
    private readonly string _directory = Path.Combine(Path.GetTempPath(), $"rtx-history-{Guid.NewGuid():N}");
    private string HistoryPath => Path.Combine(_directory, "history.json");

    [Fact]
    public void AddPersistsAndNewServiceRestoresItems()
    {
        var service = new JobHistoryService(HistoryPath);
        service.Add(CreateSnapshot("job-1", "succeeded", "movie.mp4"));

        var restored = new JobHistoryService(HistoryPath);

        var item = Assert.Single(restored.Items);
        Assert.Equal("job-1", item.Id);
        Assert.Equal("movie.mp4", item.FileName);
        Assert.Equal("succeeded", item.State);
    }

    [Fact]
    public void KeepsNewestFiftyItems()
    {
        var service = new JobHistoryService(HistoryPath);
        for (var index = 0; index < 55; index++)
        {
            service.Add(CreateSnapshot($"job-{index}", "succeeded", $"movie-{index}.mp4"));
        }

        Assert.Equal(50, service.Items.Count);
        Assert.Equal("job-54", service.Items[0].Id);
        Assert.Equal("job-5", service.Items[^1].Id);
    }

    [Fact]
    public void RemoveAndClearArePersisted()
    {
        var service = new JobHistoryService(HistoryPath);
        service.Add(CreateSnapshot("job-1", "succeeded", "one.mp4"));
        service.Add(CreateSnapshot("job-2", "failed", "two.mp4"));

        service.Remove("job-1");
        Assert.Equal("job-2", Assert.Single(new JobHistoryService(HistoryPath).Items).Id);

        service.Clear();
        Assert.Empty(new JobHistoryService(HistoryPath).Items);
    }

    [Fact]
    public void ViewModelSearchesAndFiltersPersistedItems()
    {
        var service = new JobHistoryService(HistoryPath);
        service.Add(CreateSnapshot("job-1", "succeeded", "holiday.mp4"));
        service.Add(CreateSnapshot("job-2", "failed", "work.mp4"));
        var viewModel = new HistoryViewModel(service, new StubLauncher());

        viewModel.SearchText = "holiday";
        Assert.Equal("job-1", Assert.Single(viewModel.Items).Id);

        viewModel.SearchText = string.Empty;
        viewModel.SelectedFilter = viewModel.FilterOptions.Single(option => option.State == "failed");
        Assert.Equal("job-2", Assert.Single(viewModel.Items).Id);
    }

    public void Dispose()
    {
        if (Directory.Exists(_directory))
        {
            Directory.Delete(_directory, recursive: true);
        }
    }

    private static JobSnapshot CreateSnapshot(string id, string state, string fileName) => new(
        id,
        state,
        "complete",
        1,
        100,
        100,
        59.9,
        0,
        Path.Combine("C:\\input", fileName),
        Path.Combine("C:\\output", fileName),
        [],
        null);

    private sealed class StubLauncher : IOutputLauncherService
    {
        public bool OpenOutput(string path) => true;
        public bool OpenContainingFolder(string path) => true;
    }
}
