using System.Net.Http.Json;
using System.Text.Json;
using RTXVideoConverter.Core.Models;

namespace RTXVideoConverter.Core.Services;

public sealed class BackendApiException : Exception
{
    public BackendApiException(string code, string message, string details, int statusCode, Exception? innerException = null)
        : base(message, innerException)
    {
        Code = code;
        Details = details;
        StatusCode = statusCode;
    }

    public string Code { get; }
    public string Details { get; }
    public int StatusCode { get; }
}

public sealed class BackendApiClient : IBackendApiClient
{
    private readonly HttpClient _http;
    private readonly BackendSession _session;
    private readonly JsonSerializerOptions _json = new(JsonSerializerDefaults.Web);

    public BackendApiClient(BackendSession session)
        : this(session, new HttpClientHandler())
    {
    }

    public BackendApiClient(BackendSession session, HttpMessageHandler handler)
    {
        _session = session;
        _http = new HttpClient(handler, disposeHandler: true)
        {
            Timeout = TimeSpan.FromSeconds(15),
        };
    }

    public Task<HealthResponse> GetHealthAsync(CancellationToken token = default) =>
        SendAsync<HealthResponse>(HttpMethod.Get, "/api/health", null, token);

    public Task<CapabilityResponse> GetCapabilitiesAsync(CancellationToken token = default) =>
        SendAsync<CapabilityResponse>(HttpMethod.Get, "/api/capabilities", null, token);

    public Task<MediaProbeResponse> ProbeMediaAsync(string inputPath, CancellationToken token = default) =>
        SendAsync<MediaProbeResponse>(HttpMethod.Post, "/api/media/probe", new { inputPath }, token);

    public Task<CreateJobResponse> CreateJobAsync(TranscodeRequest request, CancellationToken token = default) =>
        SendAsync<CreateJobResponse>(HttpMethod.Post, "/api/jobs", request, token);

    public Task<JobSnapshot> GetJobAsync(string id, CancellationToken token = default) =>
        SendAsync<JobSnapshot>(HttpMethod.Get, $"/api/jobs/{Uri.EscapeDataString(id)}", null, token);

    public Task CancelJobAsync(string id, CancellationToken token = default) =>
        SendAsync<object>(HttpMethod.Post, $"/api/jobs/{Uri.EscapeDataString(id)}/cancel", null, token);

    public Task ShutdownAsync(CancellationToken token = default) =>
        SendAsync<object>(HttpMethod.Post, "/api/app/shutdown", new { }, token);

    private async Task<T> SendAsync<T>(HttpMethod method, string path, object? body, CancellationToken token)
    {
        using var request = new HttpRequestMessage(method, new Uri(_session.BaseAddress, path));
        request.Headers.Add("X-App-Session-Id", _session.SessionId);
        if (body is not null)
        {
            request.Content = JsonContent.Create(body, options: _json);
        }

        HttpResponseMessage response;
        try
        {
            response = await _http.SendAsync(request, HttpCompletionOption.ResponseHeadersRead, token);
        }
        catch (Exception ex) when (ex is HttpRequestException or TaskCanceledException)
        {
            throw new BackendApiException("network_error", "后端服务不可用。", ex.Message, 0, ex);
        }

        using (response)
        {
            var text = await response.Content.ReadAsStringAsync(token);
            if (!response.IsSuccessStatusCode)
            {
                BackendErrorEnvelope? envelope = null;
                try
                {
                    envelope = JsonSerializer.Deserialize<BackendErrorEnvelope>(text, _json);
                }
                catch (JsonException)
                {
                    // Preserve the raw response below when the backend did not return its JSON envelope.
                }

                throw new BackendApiException(
                    envelope?.Error.Code ?? "http_error",
                    envelope?.Error.Message ?? $"后端返回 HTTP {(int)response.StatusCode}。",
                    envelope?.Error.Details ?? text,
                    (int)response.StatusCode);
            }

            try
            {
                return JsonSerializer.Deserialize<T>(text, _json)
                    ?? throw new JsonException("The response body was empty.");
            }
            catch (JsonException ex)
            {
                throw new BackendApiException("invalid_response", "后端返回了无效数据。", text, (int)response.StatusCode, ex);
            }
        }
    }

    public void Dispose() => _http.Dispose();
}
