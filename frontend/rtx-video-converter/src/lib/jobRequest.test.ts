import { describe, expect, it } from 'vitest';
import { buildOutputPath, buildTranscodeRequest, defaultSettings } from './jobRequest';

describe('buildOutputPath', () => {
  it('uses RTX suffix and mp4 extension', () => {
    expect(buildOutputPath('C:\\Videos\\input.mkv', 'D:\\Output')).toBe('D:\\Output\\input_RTX.mp4');
  });

  it('preserves a trailing slash on the output directory', () => {
    expect(buildOutputPath('C:\\Videos\\input.mp4', 'D:\\Output\\')).toBe('D:\\Output\\input_RTX.mp4');
  });
});

describe('buildTranscodeRequest', () => {
  it('builds VSR-only h264 request', () => {
    const request = buildTranscodeRequest({
      inputPath: 'C:\\Videos\\input.mp4',
      outputDirectory: 'C:\\Videos',
      settings: { ...defaultSettings, mode: 'vsr', codec: 'h264' },
    });

    expect(request.processing.vsr.enabled).toBe(true);
    expect(request.processing.hdr.enabled).toBe(false);
    expect(request.output.videoCodec).toBe('h264');
    expect(request.output.audioMode).toBe('copy');
    expect(request.output.subtitleMode).toBe('copy-compatible');
  });

  it('forces hevc when HDR is enabled', () => {
    const request = buildTranscodeRequest({
      inputPath: 'C:\\Videos\\input.mp4',
      outputDirectory: 'C:\\Videos',
      settings: { ...defaultSettings, mode: 'both', codec: 'h264' },
    });

    expect(request.processing.vsr.enabled).toBe(true);
    expect(request.processing.hdr.enabled).toBe(true);
    expect(request.output.videoCodec).toBe('hevc');
  });

  it('maps disabled audio and subtitles to none', () => {
    const request = buildTranscodeRequest({
      inputPath: 'C:\\Videos\\input.mp4',
      outputDirectory: 'C:\\Videos',
      settings: { ...defaultSettings, keepAudio: false, keepSubtitles: false },
    });

    expect(request.output.audioMode).toBe('none');
    expect(request.output.subtitleMode).toBe('none');
  });
});
