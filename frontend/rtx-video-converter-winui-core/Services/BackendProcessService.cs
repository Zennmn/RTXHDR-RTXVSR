using System.Diagnostics;

namespace RTXVideoConverter.Core.Services;

public sealed class BackendProcessService : IBackendProcessService
{
    private Process? _process;

    public BackendProcessService(BackendSession session)
    {
        Session = session;
    }

    public BackendSession Session { get; }
    public bool IsRunning => _process is { HasExited: false };

    public bool TryStart(out string? error)
    {
        error = null;
        if (IsRunning)
        {
            return true;
        }

        if (_process is not null)
        {
            _process.Dispose();
            _process = null;
            Session.RotatePort();
        }

        var path = ResolveBackendPath();
        if (path is null)
        {
            error = "未找到 vsr_backend.exe。请先构建后端或将它放入应用目录。";
            return false;
        }

        try
        {
            _process = Process.Start(new ProcessStartInfo
            {
                FileName = path,
                Arguments = $"--port {Session.Port} --app-session-id {Session.SessionId}",
                UseShellExecute = false,
                CreateNoWindow = true,
                WorkingDirectory = Path.GetDirectoryName(path)!,
            });

            if (_process is null)
            {
                error = "后端进程未能启动。";
                return false;
            }

            return true;
        }
        catch (Exception ex)
        {
            error = ex.Message;
            return false;
        }
    }

    private static string? ResolveBackendPath()
    {
        var candidates = new[]
        {
            Path.Combine(AppContext.BaseDirectory, "vsr_backend.exe"),
            Path.Combine(AppContext.BaseDirectory, "runtime", "vsr_backend.exe"),
            Path.GetFullPath(Path.Combine(AppContext.BaseDirectory, "..", "..", "..", "..", "..", "build", "backend-hw", "Release", "vsr_backend.exe")),
            Path.GetFullPath(Path.Combine(AppContext.BaseDirectory, "..", "..", "..", "..", "..", "build", "backend", "vsr_backend.exe")),
        };

        return candidates.FirstOrDefault(File.Exists);
    }

    public async ValueTask DisposeAsync()
    {
        if (_process is null)
        {
            return;
        }

        try
        {
            if (!_process.HasExited)
            {
                using var timeout = new CancellationTokenSource(TimeSpan.FromMilliseconds(1500));
                try
                {
                    await _process.WaitForExitAsync(timeout.Token);
                }
                catch (OperationCanceledException)
                {
                    if (!_process.HasExited)
                    {
                        _process.Kill(entireProcessTree: true);
                        await _process.WaitForExitAsync();
                    }
                }
            }
        }
        catch (InvalidOperationException)
        {
            // The process may have exited between the state check and the wait.
        }
        finally
        {
            _process.Dispose();
            _process = null;
        }
    }
}
