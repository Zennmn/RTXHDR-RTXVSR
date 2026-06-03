export type ProcessingMode = 'vsr' | 'hdr' | 'both';
export type Codec = 'H.264' | 'HEVC Main10';

export interface FileInfo {
  name: string;
  size: number;
  resolution: string;
  duration: string;
  codec: string;
}

export interface Task {
  id: string;
  filename: string;
  mode: ProcessingMode;
  status: 'Pending' | 'Processing' | 'Completed' | 'Failed' | 'Cancelled';
  progress: number;
  stage: string;
  fps: number;
  eta: string;
}
