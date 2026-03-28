import 'dart:ffi';

import 'package:ffi/ffi.dart';

final class RtxJobConfigV1 extends Struct {
  @Uint32()
  external int structSize;

  external Pointer<Utf8> inputPath;
  external Pointer<Utf8> outputPath;

  @Uint32()
  external int outputContainer;

  @Uint32()
  external int outputVideoCodec;

  @Uint32()
  external int outputAudioMode;

  @Uint32()
  external int qualityPreset;

  @Int32()
  external int targetWidth;

  @Int32()
  external int targetHeight;

  @Uint32()
  external int upscaleEnabled;

  @Uint32()
  external int upscaleFactor;

  @Uint32()
  external int hdrEnabled;

  @Uint32()
  external int trueHdrContrast;

  @Uint32()
  external int trueHdrSaturation;

  @Uint32()
  external int trueHdrMiddleGray;

  @Uint32()
  external int trueHdrMaxLuminance;

  @Uint32()
  external int videoRateControlMode;

  @Uint32()
  external int targetVideoBitrateKbps;

  @Uint32()
  external int constantQuality;

  @Int32()
  external int gpuIndex;

  @Uint32()
  external int keepAudio;

  @Uint32()
  external int overwriteOutput;

  @Uint32()
  external int loggingLevel;

  @Uint32()
  external int lockAspectRatio;
}

final class RtxSystemProbeV1 extends Struct {
  @Uint32()
  external int structSize;

  @Array(128)
  external Array<Uint8> osVersion;

  @Array(128)
  external Array<Uint8> gpuName;

  @Array(64)
  external Array<Uint8> driverVersion;

  @Array(64)
  external Array<Uint8> cudaVersion;

  @Uint32()
  external int hasNvidiaGpu;

  @Uint32()
  external int ngxVsrAvailable;

  @Uint32()
  external int ngxTruehdrAvailable;

  @Uint32()
  external int hdrExportAvailable;

  @Uint32()
  external int h264HwDecode;

  @Uint32()
  external int hevcHwDecode;

  @Uint32()
  external int av1HwDecode;

  @Uint32()
  external int h264HwEncode;

  @Uint32()
  external int hevcHwEncode;

  @Uint32()
  external int av1HwEncode;

  @Array(128)
  external Array<Uint8> supportedPixelFormats;

  @Array(256)
  external Array<Uint8> notes;
}

final class RtxMediaInfoV1 extends Struct {
  @Uint32()
  external int structSize;

  @Array(64)
  external Array<Uint8> containerName;

  @Array(32)
  external Array<Uint8> videoCodec;

  @Array(32)
  external Array<Uint8> audioCodec;

  @Array(32)
  external Array<Uint8> pixelFormat;

  @Array(32)
  external Array<Uint8> colorPrimaries;

  @Array(32)
  external Array<Uint8> transfer;

  @Array(32)
  external Array<Uint8> matrix;

  @Array(16)
  external Array<Uint8> colorRange;

  @Int32()
  external int width;

  @Int32()
  external int height;

  @Double()
  external double frameRate;

  @Double()
  external double durationSeconds;

  @Uint64()
  external int frameCount;

  @Int32()
  external int bitDepth;

  @Int32()
  external int audioChannels;

  @Int32()
  external int audioSampleRate;

  @Uint32()
  external int hasAudio;

  @Uint32()
  external int hdrSignaled;
}

final class RtxJobProgressV1 extends Struct {
  @Uint32()
  external int structSize;

  @Uint32()
  external int state;

  @Double()
  external double totalProgress;

  @Double()
  external double phaseProgress;

  @Uint64()
  external int processedFrames;

  @Uint64()
  external int totalFrames;

  @Double()
  external double fps;

  @Double()
  external double elapsedSeconds;

  @Double()
  external double etaSeconds;

  @Array(64)
  external Array<Uint8> currentPhase;

  @Array(256)
  external Array<Uint8> statusMessage;
}

final class RtxEventV1 extends Struct {
  @Uint32()
  external int severity;

  @Uint64()
  external int timestampMs;

  @Array(64)
  external Array<Uint8> phase;

  @Array(256)
  external Array<Uint8> message;
}
