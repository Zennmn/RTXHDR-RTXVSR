import type { TranscodeRequest } from '../api/types';
import type { ConversionSettings } from '../types';

export const defaultSettings: ConversionSettings = {
  mode: 'both',
  vsrScale: 2,
  vsrQuality: 4,
  hdrContrast: 100,
  hdrSaturation: 100,
  hdrMiddleGray: 44,
  hdrMaxLuminance: 1000,
  codec: 'hevc',
  keepAudio: true,
  keepSubtitles: true,
};

export function filenameStem(path: string): string {
  const normalized = path.replaceAll('/', '\\');
  const name = normalized.split('\\').filter(Boolean).at(-1) ?? 'output';
  return name.replace(/\.[^/.\\]+$/, '');
}

export function directoryName(path: string): string {
  const normalized = path.replaceAll('/', '\\');
  const index = normalized.lastIndexOf('\\');
  return index >= 0 ? normalized.slice(0, index) : '';
}

export function buildOutputPath(inputPath: string, outputDirectory: string): string {
  const separator = outputDirectory.endsWith('\\') || outputDirectory.endsWith('/') ? '' : '\\';
  return `${outputDirectory}${separator}${filenameStem(inputPath)}_RTX.mp4`;
}

export function buildTranscodeRequest({
  inputPath,
  outputDirectory,
  settings,
}: {
  inputPath: string;
  outputDirectory: string;
  settings: ConversionSettings;
}): TranscodeRequest {
  const hdrEnabled = settings.mode === 'hdr' || settings.mode === 'both';
  const vsrEnabled = settings.mode === 'vsr' || settings.mode === 'both';

  return {
    inputPath,
    outputPath: buildOutputPath(inputPath, outputDirectory),
    processing: {
      vsr: {
        enabled: vsrEnabled,
        quality: settings.vsrQuality,
        scale: settings.vsrScale,
      },
      hdr: {
        enabled: hdrEnabled,
        contrast: settings.hdrContrast,
        saturation: settings.hdrSaturation,
        middleGray: settings.hdrMiddleGray,
        maxLuminance: settings.hdrMaxLuminance,
      },
    },
    output: {
      container: 'mp4',
      videoCodec: hdrEnabled ? 'hevc' : settings.codec,
      audioMode: settings.keepAudio ? 'copy' : 'none',
      subtitleMode: settings.keepSubtitles ? 'copy-compatible' : 'none',
    },
  };
}
