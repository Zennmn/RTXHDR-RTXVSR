using RTXVideoConverter.Core.Services;
using Windows.Storage.Pickers;
using WinRT.Interop;

namespace RTXVideoConverter.WinUI.Services;

public sealed class FilePickerService : IFilePickerService
{
    private static readonly string[] VideoExtensions = [".mp4", ".mkv", ".mov", ".avi", ".webm"];
    private readonly WindowContext _windowContext;

    public FilePickerService(WindowContext windowContext)
    {
        _windowContext = windowContext;
    }

    public async Task<string?> PickVideoAsync(CancellationToken token = default)
    {
        token.ThrowIfCancellationRequested();
        var picker = new FileOpenPicker
        {
            SuggestedStartLocation = PickerLocationId.VideosLibrary,
            ViewMode = PickerViewMode.Thumbnail,
        };

        foreach (var extension in VideoExtensions)
        {
            picker.FileTypeFilter.Add(extension);
        }

        Initialize(picker);
        var file = await picker.PickSingleFileAsync();
        token.ThrowIfCancellationRequested();
        return file?.Path;
    }

    public async Task<string?> PickOutputFolderAsync(CancellationToken token = default)
    {
        token.ThrowIfCancellationRequested();
        var picker = new FolderPicker
        {
            SuggestedStartLocation = PickerLocationId.VideosLibrary,
            ViewMode = PickerViewMode.List,
        };
        picker.FileTypeFilter.Add("*");
        Initialize(picker);
        var folder = await picker.PickSingleFolderAsync();
        token.ThrowIfCancellationRequested();
        return folder?.Path;
    }

    private void Initialize(object picker)
    {
        var window = _windowContext.Window ?? throw new InvalidOperationException("The main window is not initialized.");
        InitializeWithWindow.Initialize(picker, WindowNative.GetWindowHandle(window));
    }
}
