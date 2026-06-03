import type { MediaProbeResponse, ProcessingMode, UiCodec } from './api/types';

export type { MediaProbeResponse, ProcessingMode, UiCodec };

export interface ConversionSettings {
  mode: ProcessingMode;
  vsrScale: number;
  vsrQuality: number;
  hdrContrast: number;
  hdrSaturation: number;
  hdrMiddleGray: number;
  hdrMaxLuminance: number;
  codec: UiCodec;
  keepAudio: boolean;
  keepSubtitles: boolean;
}

export interface SelectedInput {
  path: string;
  metadata: MediaProbeResponse | null;
}

// Temporary compatibility exports for the existing UI until Task 8 rewires App.tsx.
export type Codec = 'H.264' | 'HEVC Main10';

export interface FileInfo {
  name: string;
  size: number;
  resolution: string;
  duration: string;
  codec: string;
}
