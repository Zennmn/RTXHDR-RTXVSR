using Microsoft.Extensions.DependencyInjection;
using Microsoft.UI.Xaml;
using RTXVideoConverter.Core.Services;
using RTXVideoConverter.Core.ViewModels;
using RTXVideoConverter.WinUI.Services;
using RTXVideoConverter.WinUI.Views;

namespace RTXVideoConverter.WinUI;

public partial class App : Application
{
    public App()
    {
        InitializeComponent();
        Services = ConfigureServices();
        UnhandledException += OnUnhandledException;
    }

    public new static App Current => (App)Application.Current;
    public ServiceProvider Services { get; }
    public static Window? MainAppWindow { get; private set; }

    protected override void OnLaunched(LaunchActivatedEventArgs args)
    {
        try
        {
            MainAppWindow = Services.GetRequiredService<MainWindow>();
            MainAppWindow.Activate();
        }
        catch (Exception ex)
        {
            TryWriteStartupError(ex);
            throw;
        }
    }

    private static ServiceProvider ConfigureServices()
    {
        var services = new ServiceCollection();

        services.AddSingleton<BackendSession>();
        services.AddSingleton<IBackendApiClient, BackendApiClient>();
        services.AddSingleton<IBackendProcessService, BackendProcessService>();
        services.AddSingleton<IJobHistoryService, JobHistoryService>();
        services.AddSingleton<IOutputLauncherService, OutputLauncherService>();
        services.AddSingleton<IWindowPlacementStore, WindowPlacementStore>();

        services.AddSingleton<WindowContext>();
        services.AddSingleton<IFilePickerService, FilePickerService>();
        services.AddSingleton<ThemeService>();
        services.AddSingleton<IThemeService>(provider => provider.GetRequiredService<ThemeService>());

        services.AddSingleton<ConverterViewModel>();
        services.AddSingleton<HistoryViewModel>();
        services.AddSingleton<SettingsViewModel>();

        services.AddSingleton<HomePage>();
        services.AddSingleton<HistoryPage>();
        services.AddSingleton<SettingsPage>();
        services.AddSingleton<MainWindow>();

        return services.BuildServiceProvider(validateScopes: true);
    }

    private static void OnUnhandledException(object sender, Microsoft.UI.Xaml.UnhandledExceptionEventArgs args)
    {
        System.Diagnostics.Debug.WriteLine(args.Exception);
        TryWriteStartupError(args.Exception);
    }

    private static void TryWriteStartupError(Exception exception)
    {
        try
        {
            File.WriteAllText(Path.Combine(AppContext.BaseDirectory, "startup-error.txt"), exception.ToString());
        }
        catch
        {
        }
    }
}
