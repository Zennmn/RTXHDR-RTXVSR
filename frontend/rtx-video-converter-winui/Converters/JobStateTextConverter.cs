using Microsoft.UI.Xaml.Data;

namespace RTXVideoConverter.WinUI.Converters;

public sealed class JobStateTextConverter : IValueConverter
{
    public object Convert(object value, Type targetType, object parameter, string language) => value?.ToString() switch
    {
        "succeeded" => "已完成",
        "failed" => "失败",
        "canceled" => "已取消",
        "running" => "处理中",
        "queued" => "排队中",
        _ => value?.ToString() ?? string.Empty,
    };

    public object ConvertBack(object value, Type targetType, object parameter, string language) =>
        throw new NotSupportedException();
}
