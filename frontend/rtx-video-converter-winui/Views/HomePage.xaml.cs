using System.ComponentModel;
using System.Diagnostics;
using System.Numerics;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using Microsoft.UI.Xaml.Hosting;
using Microsoft.UI.Xaml.Media.Animation;
using RTXVideoConverter.Core.ViewModels;
using Windows.ApplicationModel.DataTransfer;
using Windows.Storage;
using Windows.UI.ViewManagement;

namespace RTXVideoConverter.WinUI.Views;

public sealed partial class HomePage : Page
{
    private bool _isObservingViewModel;
    private bool _modeIndicatorInitialized;
    private bool _modeIndicatorIsAnimating;
    private float _modeIndicatorFromX;
    private float _modeIndicatorToX;
    private long _modeIndicatorStartedAt;
    private Storyboard? _modeIndicatorStoryboard;
    private int _modeIndicatorAnimationVersion;

    public HomePage(ConverterViewModel viewModel)
    {
        ViewModel = viewModel;
        InitializeComponent();
        Loaded += HomePage_Loaded;
        Unloaded += HomePage_Unloaded;
    }

    public ConverterViewModel ViewModel { get; }

    private async void HomePage_Loaded(object sender, RoutedEventArgs e)
    {
        if (!_isObservingViewModel)
        {
            ViewModel.PropertyChanged += ViewModel_PropertyChanged;
            _isObservingViewModel = true;
        }

        await ViewModel.InitializeAsync();
        ApplyModeCardState(animate: false);
        UpdateFooterProgressBar();
    }

    private void HomePage_Unloaded(object sender, RoutedEventArgs e)
    {
        if (_isObservingViewModel)
        {
            ViewModel.PropertyChanged -= ViewModel_PropertyChanged;
            _isObservingViewModel = false;
        }
    }

    private void ViewModel_PropertyChanged(object? sender, PropertyChangedEventArgs e)
    {
        if (e.PropertyName == nameof(ConverterViewModel.SelectedMode))
        {
            DispatcherQueue.TryEnqueue(() => ApplyModeCardState(animate: true));
        }
        else if (e.PropertyName == nameof(ConverterViewModel.ProgressPercent))
        {
            DispatcherQueue.TryEnqueue(UpdateFooterProgressBar);
        }
    }

    private void UpdateFooterProgressBar() =>
        FooterProgressTransform.ScaleX = Math.Clamp(ViewModel.ProgressPercent / 100d, 0d, 1d);

    private void ApplyModeCardState(bool animate)
    {
        SetModeCardOpacity(VsrCard, ViewModel.IsVsrConfigurationEnabled ? 1f : 0.52f, animate);
        SetModeCardOpacity(HdrCard, ViewModel.IsHdrConfigurationEnabled ? 1f : 0.52f, animate);
        UpdateModeSelectionIndicator(animate);
    }

    private void ModeButtonGrid_SizeChanged(object sender, SizeChangedEventArgs e) =>
        UpdateModeSelectionIndicator(animate: false);

    private void LayoutRoot_SizeChanged(object sender, SizeChangedEventArgs e)
    {
        var compactHeight = e.NewSize.Height < 900;
        var compactWidth = e.NewSize.Width < 720;
        var elasticGrowth = compactHeight
            ? Math.Clamp(e.NewSize.Height - 704, 0, 160)
            : Math.Clamp(e.NewSize.Height - 900, 0, 48);
        var dropZoneGrowth = compactHeight ? 0.82 : 0.45;
        var fileInfoGrowth = compactHeight ? 0.76 : 0.35;
        var statusGrowth = compactHeight ? 0.22 : 0.12;
        var parameterGrowth = compactHeight ? 0.95 : 0.55;
        var encoderGrowth = compactHeight ? 0.62 : 0.33;

        BodyGrid.Margin = compactHeight ? new Thickness(12, 8, 20, 8) : new Thickness(12, 16, 20, 16);
        SourcePanel.Padding = compactHeight ? new Thickness(20, 8, 20, 12) : new Thickness(20, 13, 20, 20);
        SourceContentStack.Spacing = compactHeight ? 5 : 16;
        DropZoneContentStack.Spacing = compactHeight ? 4 : 10;
        InputPathStack.Spacing = OutputDirectoryStack.Spacing = OutputPreviewStack.Spacing = compactHeight ? 6 : 12;
        DropZone.MinHeight = (compactHeight ? 80 : 187) + (elasticGrowth * dropZoneGrowth);
        DropZoneIconTile.Width = DropZoneIconTile.Height = compactHeight ? 48 : 90;
        DropZoneImage.Width = DropZoneImage.Height = compactHeight ? 40 : 68;
        FileInfoCard.Padding = compactHeight ? new Thickness(6) : new Thickness(16);
        FileInfoCard.MinHeight = (compactHeight ? 146 : 208) + (elasticGrowth * fileInfoGrowth);
        FileInfoContentStack.Spacing = compactHeight ? 4 : 14;
        FileInfoGrid.RowSpacing = (compactHeight ? 4 : 12) + Math.Min(8, elasticGrowth / 24);

        WorkspacePanel.Padding = compactHeight ? new Thickness(16, 10, 16, 10) : new Thickness(16, 19, 16, 16);
        WorkspaceContentStack.Spacing = compactHeight ? 8 : 20;
        BackendStatusCard.MinHeight = (compactHeight ? 64 : 94) + (elasticGrowth * statusGrowth);
        BackendStatusCard.Padding = compactHeight ? new Thickness(14, 10, 14, 10) : new Thickness(20);
        ModeSelectorBorder.Margin = compactHeight ? new Thickness(0) : new Thickness(0, 12, 0, 0);
        VsrCard.MinHeight = HdrCard.MinHeight = (compactHeight ? 210 : 304) + (elasticGrowth * parameterGrowth);
        VsrCard.Padding = HdrCard.Padding = compactHeight ? new Thickness(12) : new Thickness(20);
        VsrContentStack.Spacing = (compactHeight ? 8 : 24) + Math.Min(12, elasticGrowth / 28);
        HdrContentStack.Spacing = (compactHeight ? 10 : 26) + Math.Min(10, elasticGrowth / 32);
        HdrFieldsGrid.RowSpacing = (compactHeight ? 10 : 30) + Math.Min(14, elasticGrowth / 22);
        VsrContentStack.VerticalAlignment = compactHeight && elasticGrowth > 20
            ? VerticalAlignment.Center
            : VerticalAlignment.Top;
        HdrContentStack.VerticalAlignment = compactHeight && elasticGrowth > 20
            ? VerticalAlignment.Center
            : VerticalAlignment.Top;
        ContrastComboBox.MinHeight = SaturationComboBox.MinHeight = MiddleGrayComboBox.MinHeight = LuminanceComboBox.MinHeight = compactHeight ? 40 : 48;
        EncoderCard.MinHeight = (compactHeight ? 132 : 203) + (elasticGrowth * encoderGrowth);
        EncoderCard.Margin = compactHeight ? new Thickness(0) : new Thickness(0, -2, 0, 0);
        EncoderCard.Padding = compactHeight ? new Thickness(12) : new Thickness(20);
        EncoderContentStack.Spacing = (compactHeight ? 2 : 11) + Math.Min(8, elasticGrowth / 34);
        EncoderContentStack.VerticalAlignment = compactHeight && elasticGrowth > 20
            ? VerticalAlignment.Center
            : VerticalAlignment.Top;
        CodecComboBox.MinHeight = ContainerComboBox.MinHeight = compactHeight ? 40 : 48;

        FooterPanel.Margin = compactHeight ? new Thickness(12, 1, 18, 8) : new Thickness(12, 1, 18, 19);
        FooterPanel.Padding = compactHeight ? new Thickness(16, 8, 14, 8) : new Thickness(20, 11, 16, 11);

        var maximumViewportSafetyInset = compactHeight ? 0 : 12;
        var viewportHeight = Math.Max(
            0,
            PageContentScrollViewer.ActualHeight -
            BodyGrid.Margin.Top -
            BodyGrid.Margin.Bottom -
            maximumViewportSafetyInset);
        if (compactHeight)
        {
            // The compact branch keeps scrolling as an accessibility fallback.
            BodyGrid.Height = double.NaN;
            BodyGrid.MinHeight = viewportHeight;
            SourcePanel.MinHeight = viewportHeight;
            WorkspacePanel.MinHeight = viewportHeight;
        }
        else
        {
            // At normal and maximized sizes the body must fit the visible viewport
            // exactly. A MinHeight here lets desired content grow the ScrollViewer's
            // extent by a few DIPs, which puts the panel border beneath the fixed footer.
            BodyGrid.MinHeight = 0;
            BodyGrid.Height = viewportHeight;
            SourcePanel.MinHeight = 0;
            WorkspacePanel.MinHeight = 0;
        }
    }

    private void UpdateModeSelectionIndicator(bool animate)
    {
        var selectedButton = ViewModel.IsVsrMode
            ? VsrModeButton
            : ViewModel.IsHdrMode
                ? HdrModeButton
                : BothModeButton;

        if (selectedButton.ActualWidth <= 0 || ModeButtonGrid.ActualWidth <= 0)
        {
            return;
        }

        var buttonOrigin = selectedButton.TransformToVisual(ModeButtonGrid)
            .TransformPoint(new Windows.Foundation.Point(0, 0));
        var targetX = (float)buttonOrigin.X + 5;
        ModeSelectionIndicator.Width = Math.Max(0, selectedButton.ActualWidth - 10);

        if (!_modeIndicatorInitialized || !animate || !AnimationsEnabled())
        {
            _modeIndicatorStoryboard?.Stop();
            ModeIndicatorTransform.X = targetX;
            _modeIndicatorFromX = targetX;
            _modeIndicatorToX = targetX;
            _modeIndicatorIsAnimating = false;
            _modeIndicatorInitialized = true;
            return;
        }

        var currentX = GetCurrentModeIndicatorX();
        _modeIndicatorStoryboard?.Stop();
        ModeIndicatorTransform.X = currentX;

        var animation = new DoubleAnimation
        {
            From = currentX,
            To = targetX,
            Duration = TimeSpan.FromMilliseconds(200),
            EasingFunction = new QuadraticEase { EasingMode = EasingMode.EaseOut },
            EnableDependentAnimation = false,
        };
        Storyboard.SetTarget(animation, ModeIndicatorTransform);
        Storyboard.SetTargetProperty(animation, nameof(ModeIndicatorTransform.X));

        var version = ++_modeIndicatorAnimationVersion;
        var storyboard = new Storyboard();
        storyboard.Children.Add(animation);
        storyboard.Completed += (_, _) =>
        {
            if (version != _modeIndicatorAnimationVersion)
            {
                return;
            }

            ModeIndicatorTransform.X = targetX;
            _modeIndicatorIsAnimating = false;
        };
        _modeIndicatorStoryboard = storyboard;
        storyboard.Begin();

        _modeIndicatorFromX = currentX;
        _modeIndicatorToX = targetX;
        _modeIndicatorStartedAt = Stopwatch.GetTimestamp();
        _modeIndicatorIsAnimating = true;
    }

    private float GetCurrentModeIndicatorX()
    {
        if (!_modeIndicatorIsAnimating)
        {
            return _modeIndicatorToX;
        }

        var elapsed = Stopwatch.GetElapsedTime(_modeIndicatorStartedAt).TotalMilliseconds;
        var progress = Math.Clamp(elapsed / 200d, 0d, 1d);
        if (progress >= 1d)
        {
            _modeIndicatorIsAnimating = false;
            return _modeIndicatorToX;
        }

        var eased = 1d - Math.Pow(1d - progress, 2d);
        return _modeIndicatorFromX + ((_modeIndicatorToX - _modeIndicatorFromX) * (float)eased);
    }

    private static void SetModeCardOpacity(FrameworkElement element, float targetOpacity, bool animate)
    {
        var visual = ElementCompositionPreview.GetElementVisual(element);
        var startOpacity = visual.Opacity;
        visual.StopAnimation(nameof(visual.Opacity));

        if (!animate || !AnimationsEnabled())
        {
            visual.Opacity = targetOpacity;
            return;
        }

        visual.Opacity = targetOpacity;
        var compositor = visual.Compositor;
        var easing = compositor.CreateCubicBezierEasingFunction(
            new System.Numerics.Vector2(0.25f, 0.1f),
            new System.Numerics.Vector2(0.25f, 1f));
        var opacity = compositor.CreateScalarKeyFrameAnimation();
        opacity.InsertKeyFrame(0, startOpacity);
        opacity.InsertKeyFrame(1, targetOpacity, easing);
        opacity.Duration = TimeSpan.FromMilliseconds(300);
        visual.StartAnimation(nameof(visual.Opacity), opacity);
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

    private void DropZone_DragOver(object sender, DragEventArgs e)
    {
        e.AcceptedOperation = DataPackageOperation.Copy;
        e.DragUIOverride.Caption = "释放以导入视频";
        e.DragUIOverride.IsContentVisible = true;
        e.DragUIOverride.IsGlyphVisible = true;
    }

    private async void DropZone_Drop(object sender, DragEventArgs e)
    {
        if (!e.DataView.Contains(StandardDataFormats.StorageItems))
        {
            return;
        }

        var items = await e.DataView.GetStorageItemsAsync();
        if (items.FirstOrDefault() is StorageFile file)
        {
            await ViewModel.LoadInputAsync(file.Path);
        }
    }
}
