using System.Net;
using System.Net.Sockets;

namespace RTXVideoConverter.Core.Services;

public sealed class BackendSession
{
    public BackendSession() : this(FindAvailablePort())
    {
    }

    public BackendSession(int port)
    {
        if (port is < 1 or > 65535)
        {
            throw new ArgumentOutOfRangeException(nameof(port));
        }

        Port = port;
        SessionId = Guid.NewGuid().ToString("D");
        BaseAddress = new Uri($"http://127.0.0.1:{port}");
    }

    public int Port { get; private set; }
    public string SessionId { get; }
    public Uri BaseAddress { get; private set; }

    public void RotatePort()
    {
        Port = FindAvailablePort();
        BaseAddress = new Uri($"http://127.0.0.1:{Port}");
    }

    private static int FindAvailablePort()
    {
        var listener = new TcpListener(IPAddress.Loopback, 0);
        listener.Start();
        try
        {
            return ((IPEndPoint)listener.LocalEndpoint).Port;
        }
        finally
        {
            listener.Stop();
        }
    }
}
