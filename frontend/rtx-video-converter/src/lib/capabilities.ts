import type { CapabilityResponse } from '../api/types';
import type { ConversionSettings } from '../types';

export function conversionCapabilityProblem(capabilities: CapabilityResponse | null, settings: ConversionSettings): string | null {
  if (capabilities === null) {
    return '正在检查硬件能力...';
  }

  const vsrEnabled = settings.mode === 'vsr' || settings.mode === 'both';
  const hdrEnabled = settings.mode === 'hdr' || settings.mode === 'both';
  const effectiveCodec = hdrEnabled ? 'hevc' : settings.codec;

  if (vsrEnabled && !capabilities.vsrAvailable) {
    return '当前系统不支持 RTX VSR。';
  }

  if (hdrEnabled && !capabilities.truehdrAvailable) {
    return '当前系统不支持 RTX HDR。';
  }

  if (effectiveCodec === 'h264' && !capabilities.nvencH264Available) {
    return '当前系统不支持 NVENC H.264 输出。';
  }

  if (effectiveCodec === 'hevc' && !capabilities.nvencHevcMain10Available) {
    return '当前系统不支持 NVENC HEVC Main10 输出。';
  }

  return null;
}
