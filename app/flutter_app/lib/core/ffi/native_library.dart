import 'dart:ffi';
import 'dart:io';

import 'package:ffi/ffi.dart';
import 'package:path/path.dart' as p;

import '../../features/job/domain/job_models.dart';
import '../../features/system/domain/system_probe_model.dart';
import '../logging/log_entry.dart';
import 'ffi_converters.dart';
import 'native_bindings.dart';

typedef _InitNative = Uint32 Function();
typedef _InitDart = int Function();
typedef _ShutdownNative = Uint32 Function();
typedef _ShutdownDart = int Function();
typedef _ProbeSystemNative = Uint32 Function(Pointer<RtxSystemProbeV1>);
typedef _ProbeSystemDart = int Function(Pointer<RtxSystemProbeV1>);
typedef _AnalyzeInputNative =
    Uint32 Function(Pointer<Utf8>, Pointer<RtxMediaInfoV1>);
typedef _AnalyzeInputDart =
    int Function(Pointer<Utf8>, Pointer<RtxMediaInfoV1>);
typedef _CreateJobNative =
    Uint32 Function(Pointer<RtxJobConfigV1>, Pointer<Uint64>);
typedef _CreateJobDart = int Function(Pointer<RtxJobConfigV1>, Pointer<Uint64>);
typedef _StartJobNative = Uint32 Function(Uint64);
typedef _StartJobDart = int Function(int);
typedef _QueryJobNative = Uint32 Function(Uint64, Pointer<RtxJobProgressV1>);
typedef _QueryJobDart = int Function(int, Pointer<RtxJobProgressV1>);
typedef _CancelJobNative = Uint32 Function(Uint64);
typedef _CancelJobDart = int Function(int);
typedef _DestroyJobNative = Uint32 Function(Uint64);
typedef _DestroyJobDart = int Function(int);
typedef _DrainEventsNative =
    Uint32 Function(Uint64, Pointer<RtxEventV1>, Uint32, Pointer<Uint32>);
typedef _DrainEventsDart =
    int Function(int, Pointer<RtxEventV1>, int, Pointer<Uint32>);
typedef _LastErrorNative = Uint32 Function(Pointer<Int8>, Uint32);
typedef _LastErrorDart = int Function(Pointer<Int8>, int);

class NativeLibrary {
  NativeLibrary._(DynamicLibrary library)
    : _init = library.lookupFunction<_InitNative, _InitDart>(
        'rtx_init_library',
      ),
      _shutdown = library.lookupFunction<_ShutdownNative, _ShutdownDart>(
        'rtx_shutdown_library',
      ),
      _probeSystem = library
          .lookupFunction<_ProbeSystemNative, _ProbeSystemDart>(
            'rtx_probe_system',
          ),
      _analyzeInput = library
          .lookupFunction<_AnalyzeInputNative, _AnalyzeInputDart>(
            'rtx_analyze_input',
          ),
      _createJob = library.lookupFunction<_CreateJobNative, _CreateJobDart>(
        'rtx_create_job',
      ),
      _startJob = library.lookupFunction<_StartJobNative, _StartJobDart>(
        'rtx_start_job',
      ),
      _queryJob = library.lookupFunction<_QueryJobNative, _QueryJobDart>(
        'rtx_query_job_progress',
      ),
      _cancelJob = library.lookupFunction<_CancelJobNative, _CancelJobDart>(
        'rtx_cancel_job',
      ),
      _destroyJob = library.lookupFunction<_DestroyJobNative, _DestroyJobDart>(
        'rtx_destroy_job',
      ),
      _drainEvents = library
          .lookupFunction<_DrainEventsNative, _DrainEventsDart>(
            'rtx_drain_job_events',
          ),
      _lastError = library.lookupFunction<_LastErrorNative, _LastErrorDart>(
        'rtx_get_last_error',
      );

  static final NativeLibrary instance = NativeLibrary._(_openLibrary());

  final _InitDart _init;
  final _ShutdownDart _shutdown;
  final _ProbeSystemDart _probeSystem;
  final _AnalyzeInputDart _analyzeInput;
  final _CreateJobDart _createJob;
  final _StartJobDart _startJob;
  final _QueryJobDart _queryJob;
  final _CancelJobDart _cancelJob;
  final _DestroyJobDart _destroyJob;
  final _DrainEventsDart _drainEvents;
  final _LastErrorDart _lastError;
  bool _initialized = false;

  static DynamicLibrary _openLibrary() {
    final executableDirectory = File(Platform.resolvedExecutable).parent.path;
    final fromEnvironment = Platform.environment['RTXHDR_NATIVE_DLL'];
    final candidates = <String>[
      if (fromEnvironment != null && fromEnvironment.isNotEmpty)
        fromEnvironment,
      p.join(executableDirectory, 'rtx_native.dll'),
      p.join(
        executableDirectory,
        '..',
        '..',
        '..',
        '..',
        '..',
        '..',
        'native',
        'out',
        'install',
        'bin',
        'rtx_native.dll',
      ),
      p.join(Directory.current.path, 'rtx_native.dll'),
      p.join(
        Directory.current.path,
        '..',
        'native',
        'out',
        'install',
        'bin',
        'rtx_native.dll',
      ),
      p.join(
        Directory.current.path,
        '..',
        '..',
        'app',
        'native',
        'out',
        'install',
        'bin',
        'rtx_native.dll',
      ),
      p.join(
        Directory.current.path,
        'app',
        'native',
        'out',
        'install',
        'bin',
        'rtx_native.dll',
      ),
    ];

    for (final candidate in candidates) {
      if (File(candidate).existsSync()) {
        return DynamicLibrary.open(candidate);
      }
    }

    return DynamicLibrary.open('rtx_native.dll');
  }

  void dispose() {
    _shutdown();
    _initialized = false;
  }

  void ensureInitialized() {
    if (_initialized) {
      return;
    }
    _throwOnError(_init(), 'initialize native library');
    _initialized = true;
  }

  SystemProbeModel probeSystem() {
    ensureInitialized();
    final probe = calloc<RtxSystemProbeV1>();
    try {
      probe.ref.structSize = sizeOf<RtxSystemProbeV1>();
      _throwOnError(_probeSystem(probe), 'probe system');
      return SystemProbeModel(
        osVersion: readFixedString(probe.ref.osVersion, 128),
        gpuName: readFixedString(probe.ref.gpuName, 128),
        driverVersion: readFixedString(probe.ref.driverVersion, 64),
        cudaVersion: readFixedString(probe.ref.cudaVersion, 64),
        hasNvidiaGpu: probe.ref.hasNvidiaGpu != 0,
        ngxVsrAvailable: probe.ref.ngxVsrAvailable != 0,
        ngxTruehdrAvailable: probe.ref.ngxTruehdrAvailable != 0,
        hdrExportAvailable: probe.ref.hdrExportAvailable != 0,
        h264HwDecode: probe.ref.h264HwDecode != 0,
        hevcHwDecode: probe.ref.hevcHwDecode != 0,
        av1HwDecode: probe.ref.av1HwDecode != 0,
        h264HwEncode: probe.ref.h264HwEncode != 0,
        hevcHwEncode: probe.ref.hevcHwEncode != 0,
        av1HwEncode: probe.ref.av1HwEncode != 0,
        supportedPixelFormats: readFixedString(
          probe.ref.supportedPixelFormats,
          128,
        ),
        notes: readFixedString(probe.ref.notes, 256),
      );
    } finally {
      calloc.free(probe);
    }
  }

  MediaInfoModel analyzeInput(String inputPath) {
    ensureInitialized();
    final pathPointer = inputPath.toNativeUtf8();
    final mediaInfo = calloc<RtxMediaInfoV1>();
    try {
      mediaInfo.ref.structSize = sizeOf<RtxMediaInfoV1>();
      _throwOnError(_analyzeInput(pathPointer, mediaInfo), 'analyze input');
      return MediaInfoModel(
        containerName: readFixedString(mediaInfo.ref.containerName, 64),
        videoCodec: readFixedString(mediaInfo.ref.videoCodec, 32),
        audioCodec: readFixedString(mediaInfo.ref.audioCodec, 32),
        pixelFormat: readFixedString(mediaInfo.ref.pixelFormat, 32),
        colorPrimaries: readFixedString(mediaInfo.ref.colorPrimaries, 32),
        transfer: readFixedString(mediaInfo.ref.transfer, 32),
        matrix: readFixedString(mediaInfo.ref.matrix, 32),
        colorRange: readFixedString(mediaInfo.ref.colorRange, 16),
        width: mediaInfo.ref.width,
        height: mediaInfo.ref.height,
        frameRate: mediaInfo.ref.frameRate,
        durationSeconds: mediaInfo.ref.durationSeconds,
        frameCount: mediaInfo.ref.frameCount,
        bitDepth: mediaInfo.ref.bitDepth,
        audioChannels: mediaInfo.ref.audioChannels,
        audioSampleRate: mediaInfo.ref.audioSampleRate,
        hasAudio: mediaInfo.ref.hasAudio != 0,
        hdrSignaled: mediaInfo.ref.hdrSignaled != 0,
      );
    } finally {
      calloc.free(pathPointer);
      calloc.free(mediaInfo);
    }
  }

  int createJob(JobConfigDraft draft, {MediaInfoModel? mediaInfo}) {
    ensureInitialized();
    final config = calloc<RtxJobConfigV1>();
    final jobId = calloc<Uint64>();
    final inputPath = draft.inputPath.toNativeUtf8();
    final outputPath = draft.outputPath.toNativeUtf8();

    try {
      config.ref
        ..structSize = sizeOf<RtxJobConfigV1>()
        ..inputPath = inputPath
        ..outputPath = outputPath
        ..outputContainer = draft.container == OutputContainer.mp4 ? 0 : 1
        ..outputVideoCodec = draft.videoCodec == OutputVideoCodec.av1 ? 1 : 0
        ..outputAudioMode = switch (draft.audioMode) {
          OutputAudioMode.copyPreferred => 0,
          OutputAudioMode.aac => 1,
          OutputAudioMode.disabled => 2,
        }
        ..qualityPreset = switch (draft.qualityPreset) {
          QualityPreset.fast => 0,
          QualityPreset.balanced => 1,
          QualityPreset.highQuality => 2,
        }
        ..targetWidth = int.tryParse(draft.customWidth) ?? 0
        ..targetHeight = int.tryParse(draft.customHeight) ?? 0
        ..upscaleEnabled = _resolveUpscaleEnabled(draft, mediaInfo)
        ..upscaleFactor = draft.resolutionMode == ResolutionMode.doubleSize
            ? 2
            : 1
        ..hdrEnabled = draft.hdrEnabled ? 1 : 0
        ..trueHdrContrast = draft.trueHdrContrast
        ..trueHdrSaturation = draft.trueHdrSaturation
        ..trueHdrMiddleGray = draft.trueHdrMiddleGray
        ..trueHdrMaxLuminance = draft.trueHdrMaxLuminance
        ..videoRateControlMode = switch (draft.videoRateControlMode) {
          VideoRateControlMode.auto => 0,
          VideoRateControlMode.targetBitrate => 1,
          VideoRateControlMode.constantQuality => 2,
        }
        ..targetVideoBitrateKbps = draft.targetVideoBitrateMbps * 1000
        ..constantQuality = draft.constantQuality
        ..gpuIndex = draft.gpuIndex
        ..keepAudio = draft.keepAudio ? 1 : 0
        ..overwriteOutput = draft.overwriteOutput ? 1 : 0
        ..loggingLevel = 2
        ..lockAspectRatio = draft.lockAspectRatio ? 1 : 0;

      _throwOnError(_createJob(config, jobId), 'create job');
      return jobId.value;
    } finally {
      calloc.free(inputPath);
      calloc.free(outputPath);
      calloc.free(config);
      calloc.free(jobId);
    }
  }

  void startJob(int jobId) {
    ensureInitialized();
    _throwOnError(_startJob(jobId), 'start job');
  }

  JobProgressModel queryProgress(int jobId) {
    ensureInitialized();
    final progress = calloc<RtxJobProgressV1>();
    try {
      progress.ref.structSize = sizeOf<RtxJobProgressV1>();
      _throwOnError(_queryJob(jobId, progress), 'query progress');
      return JobProgressModel(
        state: JobLifecycleState.values[progress.ref.state],
        totalProgress: progress.ref.totalProgress,
        phaseProgress: progress.ref.phaseProgress,
        processedFrames: progress.ref.processedFrames,
        totalFrames: progress.ref.totalFrames,
        fps: progress.ref.fps,
        elapsedSeconds: progress.ref.elapsedSeconds,
        etaSeconds: progress.ref.etaSeconds,
        currentPhase: readFixedString(progress.ref.currentPhase, 64),
        statusMessage: readFixedString(progress.ref.statusMessage, 256),
      );
    } finally {
      calloc.free(progress);
    }
  }

  List<LogEntry> drainEvents(int jobId, {int maxEvents = 32}) {
    ensureInitialized();
    final eventCount = calloc<Uint32>();
    final events = calloc<RtxEventV1>(maxEvents);
    try {
      _throwOnError(
        _drainEvents(jobId, events, maxEvents, eventCount),
        'drain job events',
      );
      final results = <LogEntry>[];
      for (var index = 0; index < eventCount.value; index += 1) {
        final event = events[index];
        results.add(
          LogEntry(
            level: switch (event.severity) {
              0 => 'error',
              1 => 'warn',
              2 => 'info',
              _ => 'debug',
            },
            phase: readFixedString(event.phase, 64),
            message: readFixedString(event.message, 256),
            timestampMs: event.timestampMs,
          ),
        );
      }
      return results;
    } finally {
      calloc.free(eventCount);
      calloc.free(events);
    }
  }

  void cancelJob(int jobId) {
    ensureInitialized();
    _throwOnError(_cancelJob(jobId), 'cancel job');
  }

  void destroyJob(int jobId) {
    ensureInitialized();
    _throwOnError(_destroyJob(jobId), 'destroy job');
  }

  int _resolveUpscaleEnabled(JobConfigDraft draft, MediaInfoModel? mediaInfo) {
    if (draft.resolutionMode == ResolutionMode.doubleSize) {
      return 1;
    }
    if (draft.resolutionMode != ResolutionMode.custom || mediaInfo == null) {
      return 0;
    }
    final width = int.tryParse(draft.customWidth) ?? 0;
    final height = int.tryParse(draft.customHeight) ?? 0;
    return width >= mediaInfo.width && height >= mediaInfo.height ? 1 : 0;
  }

  void _throwOnError(int statusCode, String action) {
    if (statusCode == 0) {
      return;
    }
    throw StateError('${_localizeAction(action)}失败: ${lastError()}');
  }

  String lastError() {
    final buffer = calloc<Int8>(1024);
    try {
      _lastError(buffer, 1024);
      return buffer.cast<Utf8>().toDartString();
    } finally {
      calloc.free(buffer);
    }
  }

  String _localizeAction(String action) {
    switch (action) {
      case 'initialize native library':
        return '初始化原生库';
      case 'probe system':
        return '检测系统能力';
      case 'analyze input':
        return '分析输入文件';
      case 'create job':
        return '创建任务';
      case 'start job':
        return '启动任务';
      case 'query progress':
        return '查询进度';
      case 'drain job events':
        return '读取任务日志';
      case 'cancel job':
        return '取消任务';
      case 'destroy job':
        return '销毁任务';
      default:
        return action;
    }
  }
}
