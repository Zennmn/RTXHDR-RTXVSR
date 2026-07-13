using System.Runtime.InteropServices;
using Microsoft.UI.Windowing;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using Microsoft.UI.Xaml.Media;
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
    private bool _closing;
    private bool _allowClose;

    public MainWindow(
        HomePage homePage,
        HistoryPage historyPage,
        SettingsPage settingsPage,
        ConverterViewModel converterViewModel,
        WindowContext windowContext,
        ThemeService themeService)
    {
        _homePage = homePage;
        _historyPage = historyPage;
        _settingsPage = settingsPage;
        _converterViewModel = converterViewModel;

        InitializeComponent();
        windowContext.Attach(this, AppRoot);
        themeService.InitializeRoot();

        SystemBackdrop = new MicaBackdrop { Kind = Microsoft.UI.Composition.SystemBackdrops.MicaKind.Base };
        ExtendsContentIntoTitleBar = true;
        SetTitleBar(AppTitleBar);
        ConfigureWindow();
        ConfigureCaptionButtons();

        RootNavigationView.SelectedItem = HomeNavigationItem;
        PageHost.Content = _homePage;
        AppWindow.Closing += MainWindow_Closing;
    }

    private void ConfigureCaptionButtons()
    {
        AppWindow.TitleBar.ButtonBackgroundColor = Microsoft.UI.Colors.Transparent;
        AppWindow.TitleBar.ButtonInactiveBackgroundColor = Microsoft.UI.Colors.Transparent;
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
        var hasQaSize = qaWidth.HasValue && qaHeight.HasValue;

        var margin = Math.Max(8, (int)Math.Round(16 * scale));
        var desiredWidth = qaWidth ?? (int)Math.Round(1440 * scale);
        var desiredHeight = qaHeight ?? (int)Math.Round(900 * scale);
        var width = hasQaSize ? desiredWidth : Math.Min(desiredWidth, Math.Max(640, workArea.Width - margin * 2));
        var height = hasQaSize ? desiredHeight : Math.Min(desiredHeight, Math.Max(480, workArea.Height - margin * 2));
        var x = workArea.X + Math.Max(0, (workArea.Width - width) / 2);
        var y = workArea.Y + Math.Max(0, (workArea.Height - height) / 2);

        AppWindow.MoveAndResize(new RectInt32(x, y, width, height));

        if (AppWindow.Presenter is OverlappedPresenter presenter)
        {
            // This is the smallest viewport in which the complete two-column workspace,
            // footer and native title bar can remain visible without clipping.
            presenter.PreferredMinimumWidth = (int)Math.Round(1200 * scale);
            presenter.PreferredMinimumHeight = (int)Math.Round(756 * scale);
        }
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
