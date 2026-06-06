import { ApiError } from '../api/backendClient';
import type { CapabilityResponse, JobSnapshot, ProcessingMode, UiCodec } from '../api/types';

const ACTIVE_STATES = new Set<JobSnapshot['state']>(['queued', 'running', 'canceling']);

export function isActiveJobSnapshot(job: JobSnapshot | null): boolean {
  return job !== null && ACTIVE_STATES.has(job.state);
}

export function errorFromJobSnapshot(job: JobSnapshot): ApiError | null {
  if (!job.error) {
    return null;
  }

  return new ApiError({
    code: job.error.code,
    message: job.error.message,
    details: job.error.details,
    status: 0,
  });
}

export function missingCapabilityReason(
  capabilities: CapabilityResponse | null,
  mode: ProcessingMode,
  codec: UiCodec,
): string | null {
  if (capabilities === null) {
    return 'Backend capabilities are not loaded.';
  }
  if ((mode === 'vsr' || mode === 'both') && !capabilities.vsrAvailable) {
    return 'VSR processing is not available.';
  }
  if ((mode === 'hdr' || mode === 'both') && !capabilities.truehdrAvailable) {
    return 'HDR processing is not available.';
  }
  if (codec === 'h264' && !capabilities.nvencH264Available) {
    return 'NVENC H.264 encoding is not available.';
  }
  if (codec === 'hevc' && !capabilities.nvencHevcMain10Available) {
    return 'NVENC HEVC Main10 encoding is not available.';
  }
  return null;
}

export function canStartConversion({
  runtime,
  backendStatus,
  loadingInput,
  inputPath,
  outputDirectory,
  hasMetadata,
  activeJob,
  capabilities,
  mode,
  codec,
}: {
  runtime: 'tauri' | 'browser';
  backendStatus: 'starting' | 'ready' | 'offline' | 'degraded';
  loadingInput: boolean;
  inputPath: string;
  outputDirectory: string;
  hasMetadata: boolean;
  activeJob: JobSnapshot | null;
  capabilities: CapabilityResponse | null;
  mode: ProcessingMode;
  codec: UiCodec;
}): boolean {
  const metadataReady = runtime === 'browser' ? true : hasMetadata;

  return (
    backendStatus !== 'offline' &&
    backendStatus !== 'starting' &&
    !loadingInput &&
    inputPath.length > 0 &&
    outputDirectory.length > 0 &&
    metadataReady &&
    !isActiveJobSnapshot(activeJob) &&
    missingCapabilityReason(capabilities, mode, codec) === null
  );
}
