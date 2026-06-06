import { describe, expect, it } from 'vitest';
import type { CapabilityResponse } from '../api/types';
import type { ConversionSettings } from '../types';
import { conversionCapabilityProblem } from './capabilities';

const readyCapabilities: CapabilityResponse = {
  d3d11Available: true,
  rtxSdkFound: true,
  vsrAvailable: true,
  truehdrAvailable: true,
  nvencH264Available: true,
  nvencHevcMain10Available: true,
  messages: [],
};

const baseSettings: ConversionSettings = {
  mode: 'vsr',
  vsrScale: 2,
  vsrQuality: 4,
  hdrContrast: 100,
  hdrSaturation: 100,
  hdrMiddleGray: 44,
  hdrMaxLuminance: 1000,
  codec: 'h264',
  keepAudio: true,
  keepSubtitles: true,
};

describe('conversionCapabilityProblem', () => {
  it('blocks starting until capabilities are loaded', () => {
    expect(conversionCapabilityProblem(null, baseSettings)).toBe('正在检查硬件能力...');
  });

  it('allows VSR with H.264 when all required capabilities are available', () => {
    expect(conversionCapabilityProblem(readyCapabilities, baseSettings)).toBeNull();
  });

  it('blocks VSR modes when VSR is unavailable', () => {
    const capabilities = { ...readyCapabilities, vsrAvailable: false };

    expect(conversionCapabilityProblem(capabilities, { ...baseSettings, mode: 'vsr' })).toBe('当前系统不支持 RTX VSR。');
    expect(conversionCapabilityProblem(capabilities, { ...baseSettings, mode: 'both', codec: 'hevc' })).toBe('当前系统不支持 RTX VSR。');
  });

  it('blocks HDR modes when TrueHDR is unavailable', () => {
    const capabilities = { ...readyCapabilities, truehdrAvailable: false };

    expect(conversionCapabilityProblem(capabilities, { ...baseSettings, mode: 'hdr', codec: 'hevc' })).toBe('当前系统不支持 RTX HDR。');
    expect(conversionCapabilityProblem(capabilities, { ...baseSettings, mode: 'both', codec: 'hevc' })).toBe('当前系统不支持 RTX HDR。');
  });

  it('blocks effective H.264 output when H.264 NVENC is unavailable', () => {
    const capabilities = { ...readyCapabilities, nvencH264Available: false };

    expect(conversionCapabilityProblem(capabilities, { ...baseSettings, mode: 'vsr', codec: 'h264' })).toBe('当前系统不支持 NVENC H.264 输出。');
  });

  it('blocks effective HEVC output when HEVC Main10 NVENC is unavailable', () => {
    const capabilities = { ...readyCapabilities, nvencHevcMain10Available: false };

    expect(conversionCapabilityProblem(capabilities, { ...baseSettings, mode: 'vsr', codec: 'hevc' })).toBe(
      '当前系统不支持 NVENC HEVC Main10 输出。',
    );
  });

  it('treats HDR and combined modes as effective HEVC output regardless of selected codec', () => {
    const capabilities = { ...readyCapabilities, nvencHevcMain10Available: false };

    expect(conversionCapabilityProblem(capabilities, { ...baseSettings, mode: 'hdr', codec: 'h264' })).toBe(
      '当前系统不支持 NVENC HEVC Main10 输出。',
    );
    expect(conversionCapabilityProblem(capabilities, { ...baseSettings, mode: 'both', codec: 'h264' })).toBe(
      '当前系统不支持 NVENC HEVC Main10 输出。',
    );
  });
});
