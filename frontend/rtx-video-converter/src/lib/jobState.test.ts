import { describe, expect, it } from 'vitest';
import { ApiError } from '../api/backendClient';
import type { CapabilityResponse, JobSnapshot } from '../api/types';
import { canStartConversion, errorFromJobSnapshot, isActiveJobSnapshot, missingCapabilityReason } from './jobState';

const capabilities: CapabilityResponse = {
  d3d11Available: true,
  rtxSdkFound: true,
  vsrAvailable: true,
  truehdrAvailable: true,
  nvencH264Available: true,
  nvencHevcMain10Available: true,
  messages: [],
};

function job(state: JobSnapshot['state']): JobSnapshot {
  return {
    id: 'job-1',
    state,
    stage: 'finalizing',
    progress: state === 'succeeded' ? 1 : 0.5,
    framesDone: 1,
    framesTotal: 1,
    fps: 0,
    etaSeconds: 0,
    inputPath: 'C:\\Videos\\input.mp4',
    outputPath: 'C:\\Videos\\input_RTX.mp4',
    warnings: [],
  };
}

describe('jobState', () => {
  it('treats terminal jobs as not active', () => {
    expect(isActiveJobSnapshot(job('running'))).toBe(true);
    expect(isActiveJobSnapshot(job('succeeded'))).toBe(false);
    expect(isActiveJobSnapshot(job('failed'))).toBe(false);
    expect(isActiveJobSnapshot(null)).toBe(false);
  });

  it('allows start after a terminal job when input metadata exists', () => {
    expect(
      canStartConversion({
        runtime: 'tauri',
        backendStatus: 'ready',
        loadingInput: false,
        inputPath: 'C:\\Videos\\input.mp4',
        outputDirectory: 'C:\\Videos',
        hasMetadata: true,
        activeJob: job('succeeded'),
        capabilities,
        mode: 'both',
        codec: 'hevc',
      }),
    ).toBe(true);
  });

  it('blocks start while a job is still active', () => {
    expect(
      canStartConversion({
        runtime: 'tauri',
        backendStatus: 'ready',
        loadingInput: false,
        inputPath: 'C:\\Videos\\input.mp4',
        outputDirectory: 'C:\\Videos',
        hasMetadata: true,
        activeJob: job('running'),
        capabilities,
        mode: 'vsr',
        codec: 'h264',
      }),
    ).toBe(false);
  });

  it('blocks start when media probe metadata is missing', () => {
    expect(
      canStartConversion({
        runtime: 'tauri',
        backendStatus: 'ready',
        loadingInput: false,
        inputPath: 'C:\\Videos\\input.mp4',
        outputDirectory: 'C:\\Videos',
        hasMetadata: false,
        activeJob: null,
        capabilities,
        mode: 'vsr',
        codec: 'h264',
      }),
    ).toBe(false);
  });

  it('allows browser-mode start without probe metadata', () => {
    expect(
      canStartConversion({
        runtime: 'browser',
        backendStatus: 'ready',
        loadingInput: false,
        inputPath: 'C:\\Videos\\input.mp4',
        outputDirectory: 'C:\\Videos',
        hasMetadata: false,
        activeJob: null,
        capabilities,
        mode: 'vsr',
        codec: 'h264',
      }),
    ).toBe(true);
  });

  it('reports missing selected capabilities', () => {
    expect(missingCapabilityReason({ ...capabilities, truehdrAvailable: false }, 'hdr', 'hevc')).toBe('HDR processing is not available.');
    expect(missingCapabilityReason({ ...capabilities, vsrAvailable: false }, 'vsr', 'h264')).toBe('VSR processing is not available.');
    expect(missingCapabilityReason({ ...capabilities, nvencH264Available: false }, 'vsr', 'h264')).toBe('NVENC H.264 encoding is not available.');
  });

  it('converts terminal job error into ApiError', () => {
    const failed = {
      ...job('failed'),
      error: { code: 'encoder_open_failed', message: 'Encoder failed.', details: 'nvenc' },
    };

    const error = errorFromJobSnapshot(failed);

    expect(error).toBeInstanceOf(ApiError);
    expect(error).toMatchObject({ code: 'encoder_open_failed', message: 'Encoder failed.', details: 'nvenc' });
  });
});
