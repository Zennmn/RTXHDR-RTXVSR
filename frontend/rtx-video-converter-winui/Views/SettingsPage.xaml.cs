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
}
