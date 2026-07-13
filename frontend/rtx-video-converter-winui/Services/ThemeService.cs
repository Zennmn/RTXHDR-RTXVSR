using Microsoft.UI.Xaml;
using RTXVideoConverter.Core.Models;
using RTXVideoConverter.Core.Services;
using Windows.UI.ViewManagement;

namespace RTXVideoConverter.WinUI.Services;

public sealed class ThemeService : IThemeService
{
    private readonly WindowContext _windowContext;
    private readonly string _settingsPath = AppDataPaths.Theme;
    private readonly string _legacySettingsPath = Path.Combine(
        Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),
        "RTXVideoConverter",
        "theme.txt");

    public ThemeService(WindowContext windowContext)
    {
        _windowContext = windowContext;
        CurrentTheme = LoadTheme();
    }

    public AppTheme CurrentTheme { get; private set; }

    public bool SystemAnimationsEnabled
    {
        get
        {
            try
            {
                return new UISettings().AnimationsEnabled;
            }
            catch
            {
                return true;
            }
        }
    }

    public void InitializeRoot() => ApplyThemeCore(CurrentTheme, persist: false);

    public void ApplyTheme(AppTheme theme) => ApplyThemeCore(theme, persist: true);

    private void ApplyThemeCore(AppTheme theme, bool persist)
    {
        CurrentTheme = theme;
        if (_windowContext.Root is { } root)
        {
            root.RequestedTheme = theme switch
            {
                AppTheme.Light => ElementTheme.Light,
                AppTheme.Dark => ElementTheme.Dark,
                _ => ElementTheme.Default,
            };
        }

        if (!persist)
        {
            return;
        }

        try
        {
            Directory.CreateDirectory(Path.GetDirectoryName(_settingsPath)!);
            File.WriteAllText(_settingsPath, theme.ToString());
        }
        catch
        {
            // Theme changes remain active for the current session if persistence is unavailable.
        }
    }

    private AppTheme LoadTheme()
    {
        try
        {
            var path = File.Exists(_settingsPath) ? _settingsPath : _legacySettingsPath;
            return File.Exists(path) &&
                   Enum.TryParse<AppTheme>(File.ReadAllText(path).Trim(), ignoreCase: true, out var value)
                ? value
                : AppTheme.System;
        }
        catch
        {
            return AppTheme.System;
        }
    }
}
