namespace RTXVideoConverter.Core.Models;

public sealed record HealthResponse(string Version, bool Ready);

public sealed record CapabilityResponse(
    bool D3d11Available,
    bool RtxSdkFound,
    bool VsrAvailable,
    bool TruehdrAvailable,
    bool NvencH264Available,
    bool NvencHevcMain10Available,
    bool NvencAv1Available,
    IReadOnlyList<string> Messages);

public sealed record MediaProbeResponse(
    string Path,
    string Name,
    long SizeBytes,
    string Resolution,
    string Duration,
    string Codec,
    IReadOnlyList<string> Warnings);

public sealed record CreateJobResponse(string Id);

public sealed record JobError(string Code, string Message, string Details);

public sealed record JobSnapshot(
    string Id,
    string State,
    string Stage,
    double Progress,
    long FramesDone,
    long FramesTotal,
    double Fps,
    long EtaSeconds,
    string InputPath,
    string OutputPath,
    IReadOnlyList<string> Warnings,
    JobError? Error);

public sealed record TranscodeRequest(
    string InputPath,
    string OutputPath,
    ProcessingOptions Processing,
    OutputOptions Output);

public sealed record ProcessingOptions(VsrOptions Vsr, HdrOptions Hdr);
public sealed record VsrOptions(bool Enabled, int Quality, double Scale);
public sealed record HdrOptions(bool Enabled, int Contrast, int Saturation, int MiddleGray, int MaxLuminance);
public sealed record OutputOptions(string Container, string VideoCodec, string AudioMode, string SubtitleMode);

internal sealed record BackendErrorEnvelope(BackendError Error);
internal sealed record BackendError(string Code, string Message, string Details);
