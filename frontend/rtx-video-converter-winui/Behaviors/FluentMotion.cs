using System.Numerics;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Hosting;
using Microsoft.UI.Xaml.Input;
using Windows.UI.ViewManagement;

namespace RTXVideoConverter.WinUI.Behaviors;

public static class FluentMotion
{
    public static readonly DependencyProperty IsHoverScaleEnabledProperty = DependencyProperty.RegisterAttached(
        "IsHoverScaleEnabled",
        typeof(bool),
        typeof(FluentMotion),
        new PropertyMetadata(false, OnIsHoverScaleEnabledChanged));

    public static bool GetIsHoverScaleEnabled(DependencyObject element) =>
        (bool)element.GetValue(IsHoverScaleEnabledProperty);

    public static void SetIsHoverScaleEnabled(DependencyObject element, bool value) =>
        element.SetValue(IsHoverScaleEnabledProperty, value);

    private static void OnIsHoverScaleEnabledChanged(DependencyObject dependencyObject, DependencyPropertyChangedEventArgs args)
    {
        if (dependencyObject is not FrameworkElement element)
        {
            return;
        }

        if (args.NewValue is true)
        {
            element.PointerEntered += OnPointerEntered;
            element.PointerExited += OnPointerExited;
            element.PointerPressed += OnPointerPressed;
            element.PointerReleased += OnPointerReleased;
        }
        else
        {
            element.PointerEntered -= OnPointerEntered;
            element.PointerExited -= OnPointerExited;
            element.PointerPressed -= OnPointerPressed;
            element.PointerReleased -= OnPointerReleased;
        }
    }

    private static void OnPointerEntered(object sender, PointerRoutedEventArgs e) => Animate(sender as FrameworkElement, 1.008f);
    private static void OnPointerExited(object sender, PointerRoutedEventArgs e) => Animate(sender as FrameworkElement, 1f);
    private static void OnPointerPressed(object sender, PointerRoutedEventArgs e) => Animate(sender as FrameworkElement, 0.996f);
    private static void OnPointerReleased(object sender, PointerRoutedEventArgs e) => Animate(sender as FrameworkElement, 1.008f);

    private static void Animate(FrameworkElement? element, float target)
    {
        if (element is null || !AnimationsEnabled())
        {
            return;
        }

        var visual = ElementCompositionPreview.GetElementVisual(element);
        visual.CenterPoint = new Vector3(
            (float)element.ActualWidth / 2,
            (float)element.ActualHeight / 2,
            0);

        var animation = visual.Compositor.CreateVector3KeyFrameAnimation();
        animation.InsertKeyFrame(
            1,
            new Vector3(target, target, 1),
            visual.Compositor.CreateCubicBezierEasingFunction(new Vector2(0, 0), new Vector2(0, 1)));
        animation.Duration = TimeSpan.FromMilliseconds(target < 1 ? 83 : 167);
        visual.StartAnimation(nameof(visual.Scale), animation);
    }

    private static bool AnimationsEnabled()
    {
        try
        {
            return new UISettings().AnimationsEnabled;
        }
        catch
        {
            return true;
        }
    }
}
