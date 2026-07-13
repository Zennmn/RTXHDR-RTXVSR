using System.Diagnostics;
using Microsoft.UI.Xaml.Controls;
using RTXVideoConverter.Core.ViewModels;

namespace RTXVideoConverter.WinUI.Views;

public sealed partial class SettingsPage : Page
{
    public SettingsPage(SettingsViewModel viewModel)
    {
        ViewModel = viewModel;
        InitializeComponent();
    }

    public SettingsViewModel ViewModel { get; }

    private async void OpenDistributionTerms_Click(object sender, Microsoft.UI.Xaml.RoutedEventArgs e)
    {
        var termsPath = Path.Combine(AppContext.BaseDirectory, "DISTRIBUTION_TERMS.txt");
        try
        {
            Process.Start(new ProcessStartInfo(termsPath) { UseShellExecute = true });
        }
        catch (Exception exception)
        {
            var dialog = new ContentDialog
            {
                XamlRoot = XamlRoot,
                Title = "无法打开分发条款",
                Content = $"请确认文件存在：{termsPath}\n\n{exception.Message}",
                CloseButtonText = "关闭"
            };
            await dialog.ShowAsync();
        }
    }
}
