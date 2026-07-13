using System.Text.Json;
using RTXVideoConverter.Core.Models;

namespace RTXVideoConverter.Core.Services;

public sealed class WindowPlacementStore : IWindowPlacementStore
{
    private static readonly JsonSerializerOptions JsonOptions = new()
    {
        PropertyNamingPolicy = JsonNamingPolicy.CamelCase,
        PropertyNameCaseInsensitive = true,
        WriteIndented = true
    };

    private readonly string _path;

    public WindowPlacementStore(string? path = null)
    {
        _path = path ?? AppDataPaths.WindowPlacement;
    }

    public WindowPlacementState? Load()
    {
        try
        {
            if (!File.Exists(_path))
            {
                return null;
            }

            var state = JsonSerializer.Deserialize<WindowPlacementState>(File.ReadAllText(_path), JsonOptions);
            return state is { IsValid: true } ? state : null;
        }
        catch
        {
            return null;
        }
    }

    public bool TrySave(WindowPlacementState state)
    {
        if (!state.IsValid)
        {
            return false;
        }

        var temporaryPath = _path + ".tmp";
        try
        {
            Directory.CreateDirectory(Path.GetDirectoryName(_path)!);
            File.WriteAllText(temporaryPath, JsonSerializer.Serialize(state, JsonOptions));
            File.Move(temporaryPath, _path, overwrite: true);
            return true;
        }
        catch
        {
            return false;
        }
        finally
        {
            try
            {
                File.Delete(temporaryPath);
            }
            catch
            {
            }
        }
    }
}
