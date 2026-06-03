export type ProcessingMode = 'vsr' | 'hdr' | 'both';
export type UiCodec = 'h264' | 'hevc';
export type JobState = 'queued' | 'running' | 'succeeded' | 'failed' | 'canceling' | 'canceled';
export type JobStage =
  | 'validating'
  | 'probing'
  | 'initializing_gpu'
  | 'decoding'
  | 'processing_rtx'
  | 'encoding'
  | 'muxing'
  | 'finalizing';

export interface BackendErrorBody {
  error: {
    code: string;
    message: string;
    details: string;
  };
}

export interface HealthResponse {
  version: string;
  ready: boolean;
}

export interface CapabilityResponse {
  d3d11Available: boolean;
  rtxSdkFound: boolean;
  vsrAvailable: boolean;
  truehdrAvailable: boolean;
  nvencH264Available: boolean;
  nvencHevcMain10Available: boolean;
  messages: string[];
}

export interface MediaProbeResponse {
  path: string;
  name: string;
  sizeBytes: number;
  resolution: string;
  duration: string;
  codec: string;
  warnings: string[];
}

export interface TranscodeRequest {
  inputPath: string;
  outputPath: string;
  processing: {
    vsr: {
      enabled: boolean;
      quality: number;
      scale: number;
    };
    hdr: {
      enabled: boolean;
      contrast: number;
      saturation: number;
      middleGray: number;
      maxLuminance: number;
    };
  };
  output: {
    container: 'mp4';
    videoCodec: UiCodec;
    audioMode: 'copy' | 'none';
    subtitleMode: 'copy-compatible' | 'none';
  };
}

export interface CreateJobResponse {
  id: string;
}

export interface JobSnapshot {
  id: string;
  state: JobState;
  stage: JobStage;
  progress: number;
  framesDone: number;
  framesTotal: number;
  fps: number;
  etaSeconds: number;
  inputPath: string;
  outputPath: string;
  warnings: string[];
  error?: {
    code: string;
    message: string;
    details: string;
  };
}
