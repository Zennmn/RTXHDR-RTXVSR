import 'package:flutter_test/flutter_test.dart';
import 'package:rtxhdr_rtxvsr/features/job/domain/job_models.dart';

void main() {
  group('JobConfigDraft', () {
    test('round-trips through json', () {
      const draft = JobConfigDraft(
        inputPath: 'C:\\video\\input.mkv',
        outputPath: 'C:\\video\\output.mp4',
        container: OutputContainer.mp4,
        videoCodec: OutputVideoCodec.av1,
        audioMode: OutputAudioMode.aac,
        qualityPreset: QualityPreset.highQuality,
        resolutionMode: ResolutionMode.custom,
        customWidth: '3840',
        customHeight: '2160',
        hdrEnabled: true,
        trueHdrContrast: 120,
        trueHdrSaturation: 110,
        trueHdrMiddleGray: 60,
        trueHdrMaxLuminance: 1500,
        videoRateControlMode: VideoRateControlMode.targetBitrate,
        targetVideoBitrateMbps: 24,
        constantQuality: 16,
        keepAudio: false,
        overwriteOutput: true,
        lockAspectRatio: false,
        gpuIndex: 1,
      );

      final restored = JobConfigDraft.fromJson(draft.toJson());

      expect(restored.toJson(), draft.toJson());
    });

    test('falls back to defaults for invalid enum indexes', () {
      final restored = JobConfigDraft.fromJson(<String, dynamic>{
        'container': 99,
        'videoCodec': -2,
        'audioMode': 99,
        'qualityPreset': 42,
        'resolutionMode': 7,
        'videoRateControlMode': 11,
      });

      expect(restored.container, OutputContainer.mkv);
      expect(restored.videoCodec, OutputVideoCodec.hevc);
      expect(restored.audioMode, OutputAudioMode.copyPreferred);
      expect(restored.qualityPreset, QualityPreset.balanced);
      expect(restored.resolutionMode, ResolutionMode.keep);
      expect(
        restored.videoRateControlMode,
        VideoRateControlMode.constantQuality,
      );
    });
  });
}
