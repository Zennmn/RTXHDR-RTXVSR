using Microsoft.UI.Xaml.Controls;
using RTXVideoConverter.Core.ViewModels;

namespace RTXVideoConverter.WinUI.Views;

public sealed partial class HistoryPage : Page
{
    public HistoryPage(HistoryViewModel viewModel)
    {
        ViewModel = viewModel;
        InitializeComponent();
    }

    public HistoryViewModel ViewModel { get; }
}
