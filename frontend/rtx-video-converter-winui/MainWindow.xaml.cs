using System.Runtime.InteropServices;
using Microsoft.UI.Windowing;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using Microsoft.UI.Xaml.Media;
using RTXVideoConverter.Core.Models;
using RTXVideoConverter.Core.Services;
using RTXVideoConverter.Core.ViewModels;
using RTXVideoConverter.WinUI.Services;
using RTXVideoConverter.WinUI.Views;
using Windows.Graphics;
using WinRT.Interop;

namespace RTXVideoConverter.WinUI;

public sealed partial class MainWindow : Window
{
    private readonly HomePage _homePage;
    private readonly HistoryPage _historyPage;
    private readonly SettingsPage _settingsPage;
    private readonly ConverterViewModel _converterViewModel;
    private readonly IWindowPlacementStore _windowPlacementStore;
    private RectInt32? _restoredBounds;
    private uint _restoredDpi = 96;
    private bool _persistWindowPlacement = true;
    private bool _closing;
    private bool _allowClose;

    public MainWindow(
        HomePage homePage,
        HistoryPage historyPage,
        SettingsPage settingsPage,
        ConverterViewModel converterViewModel,
        IWindowPlacementStore windowPlacementStore,
        WindowContext windowContext,
        ThemeService themeService)
    {
        _homePage = homePage;
        _historyPage = historyPage;
        _settingsPage = settingsPage;
        _converterViewModel = converterViewModel;
        _windowPlacementStore = windowPlacementStore;

        InitializeComponent();
        windowContext.Attach(this, AppRoot);
        themeService.InitializeRoot();

        SystemBackdrop = new MicaBackdrop { Kind = Microsoft.UI.Composition.SystemBackdrops.MicaKind.Base };
        ConfigureAppIcon();
        ExtendsContentIntoTitleBar = true;
        SetTitleBar(AppTitleBar);
        ConfigureWindow();
        ConfigureCaptionButtons();

        RootNavigationView.SelectedItem = HomeNavigationItem;
        PageHost.Content = _homePage;
        AppWindow.Changed += AppWindow_Changed;
        AppWindow.Closing += MainWindow_Closing;
    }

    private void ConfigureCaptionButtons()
    {
        AppWindow.TitleBar.ButtonBackgroundColor = Microsoft.UI.Colors.Transparent;
        AppWindow.TitleBar.ButtonInactiveBackgroundColor = Microsoft.UI.Colors.Transparent;
    }

    private void ConfigureAppIcon()
    {
        var iconPath = Path.Combine(
            AppContext.BaseDirectory,
            "Assets",
            "app-icon-rounded-v100.ico");
        if (File.Exists(iconPath))
        {
            AppWindow.SetIcon(iconPath);
        }
    }

    private void ConfigureWindow()
    {
        var hwnd = WindowNative.GetWindowHandle(this);
        var dpi = Math.Max(96u, GetDpiForWindow(hwnd));
        var scale = dpi / 96d;
        var displayArea = DisplayArea.GetFromWindowId(AppWindow.Id, DisplayAreaFallback.Primary);
        var workArea = displayArea.WorkArea;

        var qaWidth = ReadPositiveEnvironmentValue("RTX_UI_QA_WIDTH");
        var qaHeight = ReadPositiveEnvironmentValue("RTX_UI_QA_HEIGHT");
        var hasQaSize = qaWidth.HasValue || qaHeight.HasValue;
        _persistWindowPlacement = !hasQaSize;

        var savedState = hasQaSize ? null : _windowPlacementStore.Load();
        var savedScale = savedState is null ? 1d : dpi / (double)savedState.Dpi;

        var margin = Math.Max(8, (int)Math.Round(16 * scale));
        var desiredWidth = qaWidth ?? (savedState is null
            ? (int)Math.Round(1440 * scale)
            : (int)Math.Round(savedState.Width * savedScale));
        var desiredHeight = qaHeight ?? (savedState is null
            ? (int)Math.Round(900 * scale)
            : (int)Math.Round(savedState.Height * savedScale));
        var availableWidth = Math.Max(1, workArea.Width - margin * 2);
        var availableHeight = Math.Max(1, workArea.Height - margin * 2);
        var minimumWidth = Math.Min((int)Math.Round(1200 * scale), availableWidth);
        var minimumHeight = Math.Min((int)Math.Round(756 * scale), availableHeight);
        var width = Math.Clamp(desiredWidth, minimumWidth, availableWidth);
        var height = Math.Clamp(desiredHeight, minimumHeight, availableHeight);
        var x = workArea.X + Math.Max(0, (workArea.Width - width) / 2);
        var y = workArea.Y + Math.Max(0, (workArea.Height - height) / 2);

        _restoredBounds = new RectInt32(x, y, width, height);
        _restoredDpi = dpi;
        AppWindow.MoveAndResize(_restoredBounds.Value);

        if (AppWindow.Presenter is OverlappedPresenter presenter)
        {
            // This is the smallest viewport in which the complete two-column workspace,
            // footer and native title bar can remain visible without clipping.
            presenter.PreferredMinimumWidth = minimumWidth;
            presenter.PreferredMinimumHeight = minimumHeight;
            if (savedState?.IsMaximized == true)
            {
                presenter.Maximize();
            }
        }
    }

    private void AppWindow_Changed(AppWindow sender, AppWindowChangedEventArgs args)
    {
        if (sender.Presenter is not OverlappedPresenter { State: OverlappedPresenterState.Restored } ||
            (!args.DidPositionChange && !args.DidSizeChange && !args.DidPresenterChange))
        {
            return;
        }

        _restoredBounds = new RectInt32(sender.Position.X, sender.Position.Y, sender.Size.Width, sender.Size.Height);
        _restoredDpi = Math.Max(96u, GetDpiForWindow(WindowNative.GetWindowHandle(this)));
    }

    private void SaveWindowPlacement()
    {
        if (!_persistWindowPlacement || _restoredBounds is not { } bounds)
        {
            return;
        }

        var isMaximized = AppWindow.Presenter is OverlappedPresenter
        {
            State: OverlappedPresenterState.Maximized
        };
        _windowPlacementStore.TrySave(new WindowPlacementState(
            bounds.Width,
            bounds.Height,
            checked((int)_restoredDpi),
            isMaximized));
    }

    private static int? ReadPositiveEnvironmentValue(string name) =>
        int.TryParse(Environment.GetEnvironmentVariable(name), out var value) && value > 0 ? value : null;

    private void PaneToggleButton_Click(object sender, RoutedEventArgs e)
    {
        RootNavigationView.IsPaneOpen = !RootNavigationView.IsPaneOpen;
    }

    private void RootNavigationView_ItemInvoked(NavigationView sender, NavigationViewItemInvokedEventArgs args)
    {
        if (args.IsSettingsInvoked)
        {
            ShowSettings();
            return;
        }

        if (args.InvokedItemContainer is NavigationViewItem item)
        {
            switch (item.Tag?.ToString())
            {
                case "history":
                    PageHost.Content = _historyPage;
                    break;
                case "settings":
                    ShowSettings();
                    break;
                default:
                    PageHost.Content = _homePage;
                    break;
            }
        }
    }

    private void ShowSettings()
    {
        RootNavigationView.SelectedItem = SettingsNavigationItem;
        PageHost.Content = _settingsPage;
    }

    private async void MainWindow_Closing(AppWindow sender, AppWindowClosingEventArgs args)
    {
        if (_allowClose)
        {
            return;
        }

        args.Cancel = true;
        if (_closing)
        {
            return;
        }

        _closing = true;
        SaveWindowPlacement();
        try
        {
            await _converterViewModel.DisposeAsync();
            await App.Current.Services.DisposeAsync();
        }
        finally
        {
            _allowClose = true;
            Close();
        }
    }

    [DllImport("user32.dll")]
    private static extern uint GetDpiForWindow(IntPtr hwnd);
}
