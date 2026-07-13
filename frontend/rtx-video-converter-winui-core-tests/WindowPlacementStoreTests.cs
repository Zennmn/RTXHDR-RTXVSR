using RTXVideoConverter.Core.Models;
using RTXVideoConverter.Core.Services;
using Xunit;

namespace RTXVideoConverter.Core.Tests;

public sealed class WindowPlacementStoreTests : IDisposable
{
    private readonly string _directory = Path.Combine(
        Path.GetTempPath(),
        "rtx-video-converter-tests",
        Guid.NewGuid().ToString("N"));

    [Fact]
    public void SavesAndLoadsPlacementState()
    {
        var path = Path.Combine(_directory, "window-placement.json");
        var store = new WindowPlacementStore(path);
        var expected = new WindowPlacementState(1440, 900, 120, true);

        Assert.True(store.TrySave(expected));

        Assert.Equal(expected, store.Load());
    }

    [Fact]
    public void ReturnsNullForMalformedOrInvalidState()
    {
        Directory.CreateDirectory(_directory);
        var path = Path.Combine(_directory, "window-placement.json");
        var store = new WindowPlacementStore(path);

        File.WriteAllText(path, "not-json");
        Assert.Null(store.Load());

        File.WriteAllText(path, "{\"width\":0,\"height\":900,\"dpi\":120,\"isMaximized\":false}");
        Assert.Null(store.Load());
    }

    public void Dispose()
    {
        if (Directory.Exists(_directory))
        {
            Directory.Delete(_directory, recursive: true);
        }
    }
}
