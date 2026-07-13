using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Data;
using Microsoft.UI.Xaml.Media;
using RTXVideoConverter.Core.Models;

namespace RTXVideoConverter.WinUI.Converters;

public sealed class BackendStatusBrushConverter : IValueConverter
{
    public object Convert(object value, Type targetType, object parameter, string language)
    {
        var key = value is BackendStatus status
            ? status switch
            {
                BackendStatus.Ready => "StatusSuccessBrush",
                BackendStatus.Degraded => "StatusWarningBrush",
                BackendStatus.Offline => "StatusErrorBrush",
                _ => "StatusNeutralBrush",
            }
            : "StatusNeutralBrush";

        return Application.Current.Resources.TryGetValue(key, out var brush)
            ? brush
            : new SolidColorBrush(Microsoft.UI.Colors.Gray);
    }

    public object ConvertBack(object value, Type targetType, object parameter, string language) =>
        throw new NotSupportedException();
}
