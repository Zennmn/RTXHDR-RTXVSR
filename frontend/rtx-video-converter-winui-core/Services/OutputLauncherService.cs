using System.Diagnostics;

namespace RTXVideoConverter.Core.Services;

public sealed class OutputLauncherService : IOutputLauncherService
{
    public bool OpenOutput(string path)
    {
        if (!File.Exists(path))
        {
            return false;
        }

        return Start(path);
    }

    public bool OpenContainingFolder(string path)
    {
        var directory = File.Exists(path) ? Path.GetDirectoryName(path) : path;
        if (string.IsNullOrWhiteSpace(directory) || !Directory.Exists(directory))
        {
            return false;
        }

        var info = new ProcessStartInfo("explorer.exe") { UseShellExecute = false };
        if (File.Exists(path))
        {
            info.ArgumentList.Add($"/select,{path}");
        }
        else
        {
            info.ArgumentList.Add(directory);
        }

        return Process.Start(info) is not null;
    }

    private static bool Start(string path) => Process.Start(new ProcessStartInfo(path)
    {
        UseShellExecute = true,
    }) is not null;
}
