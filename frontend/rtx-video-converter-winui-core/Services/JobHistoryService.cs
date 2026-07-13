using System.Collections.ObjectModel;
using System.Text.Json;
using RTXVideoConverter.Core.Models;

namespace RTXVideoConverter.Core.Services;

public sealed class JobHistoryService : IJobHistoryService
{
    private const int MaximumItems = 50;
    private readonly ObservableCollection<JobHistoryItem> _items = [];
    private readonly string _storagePath;
    private readonly JsonSerializerOptions _jsonOptions = new(JsonSerializerDefaults.Web)
    {
        WriteIndented = true,
    };

    public JobHistoryService(string? storagePath = null)
    {
        _storagePath = storagePath ?? AppDataPaths.History;
        Items = new ReadOnlyObservableCollection<JobHistoryItem>(_items);
        Load();
    }

    public ReadOnlyObservableCollection<JobHistoryItem> Items { get; }

    public void Add(JobSnapshot snapshot)
    {
        _items.Insert(0, new JobHistoryItem(
            snapshot.Id,
            Path.GetFileName(snapshot.InputPath),
            snapshot.OutputPath,
            snapshot.State,
            DateTimeOffset.Now,
            snapshot.Fps));

        while (_items.Count > MaximumItems)
        {
            _items.RemoveAt(_items.Count - 1);
        }

        Save();
    }

    public void Remove(string id)
    {
        var item = _items.FirstOrDefault(candidate => string.Equals(candidate.Id, id, StringComparison.Ordinal));
        if (item is null)
        {
            return;
        }

        _items.Remove(item);
        Save();
    }

    public void Clear()
    {
        if (_items.Count == 0)
        {
            return;
        }

        _items.Clear();
        Save();
    }

    private void Load()
    {
        try
        {
            if (!File.Exists(_storagePath))
            {
                return;
            }

            var items = JsonSerializer.Deserialize<List<JobHistoryItem>>(File.ReadAllText(_storagePath), _jsonOptions);
            if (items is null)
            {
                return;
            }

            foreach (var item in items.OrderByDescending(item => item.CompletedAt).Take(MaximumItems))
            {
                _items.Add(item);
            }
        }
        catch (JsonException)
        {
            // Ignore malformed history. A valid file is written after the next completed task.
        }
        catch (IOException)
        {
            // History must never prevent the application from starting.
        }
        catch (UnauthorizedAccessException)
        {
        }
    }

    private void Save()
    {
        try
        {
            var directory = Path.GetDirectoryName(_storagePath)!;
            Directory.CreateDirectory(directory);
            var temporaryPath = _storagePath + ".tmp";
            File.WriteAllText(temporaryPath, JsonSerializer.Serialize(_items, _jsonOptions));
            File.Move(temporaryPath, _storagePath, overwrite: true);
        }
        catch (IOException)
        {
        }
        catch (UnauthorizedAccessException)
        {
        }
    }
}
