using CommunityToolkit.Mvvm.ComponentModel;
using RTXVideoConverter.Core.Models;
using RTXVideoConverter.Core.Services;

namespace RTXVideoConverter.Core.ViewModels;

public partial class SettingsViewModel : ObservableObject
{
    private readonly IThemeService _themeService;
    private bool _initializing = true;

    public SettingsViewModel(IThemeService themeService)
    {
        _themeService = themeService;
        SelectedThemeIndex = (int)themeService.CurrentTheme;
        _initializing = false;
    }

    [ObservableProperty] private int selectedThemeIndex;

    public bool SystemAnimationsEnabled => _themeService.SystemAnimationsEnabled;
    public string AnimationStatus => SystemAnimationsEnabled
        ? "系统动画已开启，界面使用 Fluent 硬件加速过渡。"
        : "系统已减少动画，界面将自动使用静态状态切换。";

    partial void OnSelectedThemeIndexChanged(int value)
    {
        if (!_initializing && Enum.IsDefined(typeof(AppTheme), value))
        {
            _themeService.ApplyTheme((AppTheme)value);
        }
    }
}
