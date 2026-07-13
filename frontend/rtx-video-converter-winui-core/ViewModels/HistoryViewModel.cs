using System.Collections.ObjectModel;
using System.Collections.Specialized;
using CommunityToolkit.Mvvm.ComponentModel;
using CommunityToolkit.Mvvm.Input;
using RTXVideoConverter.Core.Models;
using RTXVideoConverter.Core.Services;

namespace RTXVideoConverter.Core.ViewModels;

public sealed record HistoryFilterOption(string Label, string? State);

public partial class HistoryViewModel : ObservableObject
{
    private readonly IJobHistoryService _history;
    private readonly IOutputLauncherService _launcher;
    private readonly ObservableCollection<JobHistoryItem> _filteredItems = [];

    public HistoryViewModel(IJobHistoryService history, IOutputLauncherService launcher)
    {
        _history = history;
        _launcher = launcher;
        Items = new ReadOnlyObservableCollection<JobHistoryItem>(_filteredItems);
        FilterOptions =
        [
            new("全部状态", null),
            new("已完成", "succeeded"),
            new("失败", "failed"),
            new("已取消", "canceled"),
        ];
        selectedFilter = FilterOptions[0];
        ((INotifyCollectionChanged)_history.Items).CollectionChanged += OnSourceCollectionChanged;
        RebuildItems();
    }

    public ReadOnlyObservableCollection<JobHistoryItem> Items { get; }
    public IReadOnlyList<HistoryFilterOption> FilterOptions { get; }

    [ObservableProperty] private string searchText = string.Empty;
    [ObservableProperty] private HistoryFilterOption selectedFilter;

    public bool HasItems => Items.Count > 0;
    public bool HasNoItems => !HasItems;
    public bool HasStoredItems => _history.Items.Count > 0;
    public string EmptyTitle => HasStoredItems ? "没有匹配的任务" : "还没有任务记录";
    public string EmptyDescription => HasStoredItems
        ? "尝试更改搜索内容或状态筛选。"
        : "完成一次视频转换后，任务状态、输出位置和处理速度会显示在这里。";

    partial void OnSearchTextChanged(string value) => RebuildItems();
    partial void OnSelectedFilterChanged(HistoryFilterOption value) => RebuildItems();

    [RelayCommand(CanExecute = nameof(HasStoredItems))]
    private void Clear() => _history.Clear();

    [RelayCommand]
    private void Remove(JobHistoryItem? item)
    {
        if (item is not null)
        {
            _history.Remove(item.Id);
        }
    }

    [RelayCommand]
    private void OpenOutput(JobHistoryItem? item)
    {
        if (item is not null)
        {
            _launcher.OpenOutput(item.OutputPath);
        }
    }

    [RelayCommand]
    private void OpenContainingFolder(JobHistoryItem? item)
    {
        if (item is not null)
        {
            _launcher.OpenContainingFolder(item.OutputPath);
        }
    }

    private void OnSourceCollectionChanged(object? sender, NotifyCollectionChangedEventArgs e) => RebuildItems();

    private void RebuildItems()
    {
        var query = SearchText.Trim();
        var state = SelectedFilter?.State;
        var matches = _history.Items.Where(item =>
            (state is null || string.Equals(item.State, state, StringComparison.Ordinal)) &&
            (query.Length == 0 ||
             item.FileName.Contains(query, StringComparison.CurrentCultureIgnoreCase) ||
             item.OutputPath.Contains(query, StringComparison.CurrentCultureIgnoreCase)));

        _filteredItems.Clear();
        foreach (var item in matches)
        {
            _filteredItems.Add(item);
        }

        OnPropertyChanged(nameof(HasItems));
        OnPropertyChanged(nameof(HasNoItems));
        OnPropertyChanged(nameof(HasStoredItems));
        OnPropertyChanged(nameof(EmptyTitle));
        OnPropertyChanged(nameof(EmptyDescription));
        ClearCommand.NotifyCanExecuteChanged();
    }
}
