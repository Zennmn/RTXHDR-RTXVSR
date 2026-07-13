using Microsoft.UI.Xaml;

namespace RTXVideoConverter.WinUI.Services;

public sealed class WindowContext
{
    public Window? Window { get; private set; }
    public FrameworkElement? Root { get; private set; }

    public void Attach(Window window, FrameworkElement root)
    {
        Window = window;
        Root = root;
    }
}
