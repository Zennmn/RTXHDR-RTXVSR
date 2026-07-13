using System.Net;
using System.Text;
using RTXVideoConverter.Core.Services;
using Xunit;

namespace RTXVideoConverter.Core.Tests;

public sealed class BackendApiClientTests
{
    [Fact]
    public async Task SendsSessionHeaderWithoutDisclosingItInTheUrl()
    {
        var session = new BackendSession(49321);
        var handler = new RecordingHandler(HttpStatusCode.OK, "{\"version\":\"test\",\"ready\":true}");
        using var client = new BackendApiClient(session, handler);

        await client.GetHealthAsync();

        Assert.Equal(session.SessionId, handler.Request!.Headers.GetValues("X-App-Session-Id").Single());
        Assert.DoesNotContain(session.SessionId, handler.Request.RequestUri!.ToString(), StringComparison.Ordinal);
    }

    [Fact]
    public async Task MapsBackendErrorEnvelope()
    {
        var handler = new RecordingHandler(
            HttpStatusCode.BadRequest,
            "{\"error\":{\"code\":\"invalid_request\",\"message\":\"Bad request\",\"details\":\"detail\"}}");
        using var client = new BackendApiClient(new BackendSession(49321), handler);

        var error = await Assert.ThrowsAsync<BackendApiException>(() => client.GetCapabilitiesAsync());

        Assert.Equal("invalid_request", error.Code);
        Assert.Equal("detail", error.Details);
        Assert.Equal(400, error.StatusCode);
    }

    [Fact]
    public async Task DeserializesAv1EncoderCapability()
    {
        var handler = new RecordingHandler(
            HttpStatusCode.OK,
            "{\"d3d11Available\":true,\"rtxSdkFound\":true,\"vsrAvailable\":true,\"truehdrAvailable\":true," +
            "\"nvencH264Available\":true,\"nvencHevcMain10Available\":true,\"nvencAv1Available\":true,\"messages\":[]}");
        using var client = new BackendApiClient(new BackendSession(49321), handler);

        var capabilities = await client.GetCapabilitiesAsync();

        Assert.True(capabilities.NvencAv1Available);
    }

    private sealed class RecordingHandler(HttpStatusCode statusCode, string body) : HttpMessageHandler
    {
        public HttpRequestMessage? Request { get; private set; }

        protected override Task<HttpResponseMessage> SendAsync(HttpRequestMessage request, CancellationToken cancellationToken)
        {
            Request = request;
            return Task.FromResult(new HttpResponseMessage(statusCode)
            {
                Content = new StringContent(body, Encoding.UTF8, "application/json"),
            });
        }
    }
}
