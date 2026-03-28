import '../../../core/logging/log_entry.dart';

enum OutputContainer { mp4, mkv }

enum OutputVideoCodec { hevc, av1 }

enum OutputAudioMode { copyPreferred, aac, disabled }

enum QualityPreset { fast, balanced, highQuality }

enum VideoRateControlMode { auto, targetBitrate, constantQuality }

enum ResolutionMode { keep, doubleSize, custom }

enum JobLifecycleState {
  created,
  queued,
  analyzing,
  preparing,
  running,
  completed,
  failed,
  cancelled,
}

extension JobLifecycleStateLocalization on JobLifecycleState {
  String get localizedLabel {
    switch (this) {
      case JobLifecycleState.created:
        return '已创建';
      case JobLifecycleState.queued:
        return '已排队';
      case JobLifecycleState.analyzing:
        return '分析中';
      case JobLifecycleState.preparing:
        return '准备中';
      case JobLifecycleState.running:
        return '处理中';
      case JobLifecycleState.completed:
        return '已完成';
      case JobLifecycleState.failed:
        return '失败';
      case JobLifecycleState.cancelled:
        return '已取消';
    }
  }
}

String localizeRuntimeText(String value) {
  switch (value) {
    case 'Idle':
      return '空闲';
    case 'Ready':
      return '就绪';
    case 'Created':
      return '已创建';
    case 'Job created':
      return '任务已创建';
    case 'Queued':
      return '已排队';
    case 'Job queued':
      return '任务已加入队列';
    case 'Probe':
      return '媒体探测';
    case 'Analyzing input':
      return '正在分析输入文件';
    case 'Validation':
      return '配置校验';
    case 'Validating job configuration':
      return '正在校验任务配置';
    case 'Backend':
      return '增强后端';
    case 'Creating enhancement backend':
      return '正在创建增强后端';
    case 'Processing':
      return '帧处理';
    case 'Processing frames':
      return '正在处理视频帧';
    case 'Frame processing in progress':
      return '正在持续处理视频帧';
    case 'Finished':
      return '已结束';
    case 'Completed':
      return '已完成';
    case 'Validating output file':
      return '正在校验输出文件';
    case 'Export completed successfully':
      return '导出已成功完成';
    case 'Cancellation requested':
      return '已请求取消任务';
    case 'Job finished successfully':
      return '任务已成功完成';
    default:
      return value;
  }
}

class MediaInfoModel {
  const MediaInfoModel({
    required this.containerName,
    required this.videoCodec,
    required this.audioCodec,
    required this.pixelFormat,
    required this.colorPrimaries,
    required this.transfer,
    required this.matrix,
    required this.colorRange,
    required this.width,
    required this.height,
    required this.frameRate,
    required this.durationSeconds,
    required this.frameCount,
    required this.bitDepth,
    required this.audioChannels,
    required this.audioSampleRate,
    required this.hasAudio,
    required this.hdrSignaled,
  });

  final String containerName;
  final String videoCodec;
  final String audioCodec;
  final String pixelFormat;
  final String colorPrimaries;
  final String transfer;
  final String matrix;
  final String colorRange;
  final int width;
  final int height;
  final double frameRate;
  final double durationSeconds;
  final int frameCount;
  final int bitDepth;
  final int audioChannels;
  final int audioSampleRate;
  final bool hasAudio;
  final bool hdrSignaled;
}

class JobProgressModel {
  const JobProgressModel({
    required this.state,
    required this.totalProgress,
    required this.phaseProgress,
    required this.processedFrames,
    required this.totalFrames,
    required this.fps,
    required this.elapsedSeconds,
    required this.etaSeconds,
    required this.currentPhase,
    required this.statusMessage,
  });

  final JobLifecycleState state;
  final double totalProgress;
  final double phaseProgress;
  final int processedFrames;
  final int totalFrames;
  final double fps;
  final double elapsedSeconds;
  final double etaSeconds;
  final String currentPhase;
  final String statusMessage;

  bool get isTerminal =>
      state == JobLifecycleState.completed ||
      state == JobLifecycleState.failed ||
      state == JobLifecycleState.cancelled;
}

class JobConfigDraft {
  const JobConfigDraft({
    this.inputPath = '',
    this.outputPath = '',
    this.container = OutputContainer.mkv,
    this.videoCodec = OutputVideoCodec.hevc,
    this.audioMode = OutputAudioMode.copyPreferred,
    this.qualityPreset = QualityPreset.balanced,
    this.resolutionMode = ResolutionMode.keep,
    this.customWidth = '',
    this.customHeight = '',
    this.hdrEnabled = false,
    this.trueHdrContrast = 100,
    this.trueHdrSaturation = 100,
    this.trueHdrMiddleGray = 50,
    this.trueHdrMaxLuminance = 1000,
    this.videoRateControlMode = VideoRateControlMode.constantQuality,
    this.targetVideoBitrateMbps = 12,
    this.constantQuality = 18,
    this.keepAudio = true,
    this.overwriteOutput = false,
    this.lockAspectRatio = true,
    this.gpuIndex = 0,
  });

  final String inputPath;
  final String outputPath;
  final OutputContainer container;
  final OutputVideoCodec videoCodec;
  final OutputAudioMode audioMode;
  final QualityPreset qualityPreset;
  final ResolutionMode resolutionMode;
  final String customWidth;
  final String customHeight;
  final bool hdrEnabled;
  final int trueHdrContrast;
  final int trueHdrSaturation;
  final int trueHdrMiddleGray;
  final int trueHdrMaxLuminance;
  final VideoRateControlMode videoRateControlMode;
  final int targetVideoBitrateMbps;
  final int constantQuality;
  final bool keepAudio;
  final bool overwriteOutput;
  final bool lockAspectRatio;
  final int gpuIndex;

  Map<String, Object?> toJson() {
    return <String, Object?>{
      'inputPath': inputPath,
      'outputPath': outputPath,
      'container': container.index,
      'videoCodec': videoCodec.index,
      'audioMode': audioMode.index,
      'qualityPreset': qualityPreset.index,
      'resolutionMode': resolutionMode.index,
      'customWidth': customWidth,
      'customHeight': customHeight,
      'hdrEnabled': hdrEnabled,
      'trueHdrContrast': trueHdrContrast,
      'trueHdrSaturation': trueHdrSaturation,
      'trueHdrMiddleGray': trueHdrMiddleGray,
      'trueHdrMaxLuminance': trueHdrMaxLuminance,
      'videoRateControlMode': videoRateControlMode.index,
      'targetVideoBitrateMbps': targetVideoBitrateMbps,
      'constantQuality': constantQuality,
      'keepAudio': keepAudio,
      'overwriteOutput': overwriteOutput,
      'lockAspectRatio': lockAspectRatio,
      'gpuIndex': gpuIndex,
    };
  }

  factory JobConfigDraft.fromJson(Map<String, dynamic> json) {
    return JobConfigDraft(
      inputPath: _readString(json, 'inputPath'),
      outputPath: _readString(json, 'outputPath'),
      container: _readEnum(
        json,
        'container',
        OutputContainer.values,
        OutputContainer.mkv,
      ),
      videoCodec: _readEnum(
        json,
        'videoCodec',
        OutputVideoCodec.values,
        OutputVideoCodec.hevc,
      ),
      audioMode: _readEnum(
        json,
        'audioMode',
        OutputAudioMode.values,
        OutputAudioMode.copyPreferred,
      ),
      qualityPreset: _readEnum(
        json,
        'qualityPreset',
        QualityPreset.values,
        QualityPreset.balanced,
      ),
      resolutionMode: _readEnum(
        json,
        'resolutionMode',
        ResolutionMode.values,
        ResolutionMode.keep,
      ),
      customWidth: _readString(json, 'customWidth'),
      customHeight: _readString(json, 'customHeight'),
      hdrEnabled: _readBool(json, 'hdrEnabled'),
      trueHdrContrast: _readInt(json, 'trueHdrContrast', 100),
      trueHdrSaturation: _readInt(json, 'trueHdrSaturation', 100),
      trueHdrMiddleGray: _readInt(json, 'trueHdrMiddleGray', 50),
      trueHdrMaxLuminance: _readInt(json, 'trueHdrMaxLuminance', 1000),
      videoRateControlMode: _readEnum(
        json,
        'videoRateControlMode',
        VideoRateControlMode.values,
        VideoRateControlMode.constantQuality,
      ),
      targetVideoBitrateMbps: _readInt(json, 'targetVideoBitrateMbps', 12),
      constantQuality: _readInt(json, 'constantQuality', 18),
      keepAudio: _readBool(json, 'keepAudio', fallback: true),
      overwriteOutput: _readBool(json, 'overwriteOutput'),
      lockAspectRatio: _readBool(json, 'lockAspectRatio', fallback: true),
      gpuIndex: _readInt(json, 'gpuIndex', 0),
    );
  }

  JobConfigDraft copyWith({
    String? inputPath,
    String? outputPath,
    OutputContainer? container,
    OutputVideoCodec? videoCodec,
    OutputAudioMode? audioMode,
    QualityPreset? qualityPreset,
    ResolutionMode? resolutionMode,
    String? customWidth,
    String? customHeight,
    bool? hdrEnabled,
    int? trueHdrContrast,
    int? trueHdrSaturation,
    int? trueHdrMiddleGray,
    int? trueHdrMaxLuminance,
    VideoRateControlMode? videoRateControlMode,
    int? targetVideoBitrateMbps,
    int? constantQuality,
    bool? keepAudio,
    bool? overwriteOutput,
    bool? lockAspectRatio,
    int? gpuIndex,
  }) {
    return JobConfigDraft(
      inputPath: inputPath ?? this.inputPath,
      outputPath: outputPath ?? this.outputPath,
      container: container ?? this.container,
      videoCodec: videoCodec ?? this.videoCodec,
      audioMode: audioMode ?? this.audioMode,
      qualityPreset: qualityPreset ?? this.qualityPreset,
      resolutionMode: resolutionMode ?? this.resolutionMode,
      customWidth: customWidth ?? this.customWidth,
      customHeight: customHeight ?? this.customHeight,
      hdrEnabled: hdrEnabled ?? this.hdrEnabled,
      trueHdrContrast: trueHdrContrast ?? this.trueHdrContrast,
      trueHdrSaturation: trueHdrSaturation ?? this.trueHdrSaturation,
      trueHdrMiddleGray: trueHdrMiddleGray ?? this.trueHdrMiddleGray,
      trueHdrMaxLuminance: trueHdrMaxLuminance ?? this.trueHdrMaxLuminance,
      videoRateControlMode: videoRateControlMode ?? this.videoRateControlMode,
      targetVideoBitrateMbps:
          targetVideoBitrateMbps ?? this.targetVideoBitrateMbps,
      constantQuality: constantQuality ?? this.constantQuality,
      keepAudio: keepAudio ?? this.keepAudio,
      overwriteOutput: overwriteOutput ?? this.overwriteOutput,
      lockAspectRatio: lockAspectRatio ?? this.lockAspectRatio,
      gpuIndex: gpuIndex ?? this.gpuIndex,
    );
  }
}

String _readString(Map<String, dynamic> json, String key) {
  final value = json[key];
  return value is String ? value : '';
}

int _readInt(Map<String, dynamic> json, String key, int fallback) {
  final value = json[key];
  if (value is int) {
    return value;
  }
  if (value is num) {
    return value.toInt();
  }
  return fallback;
}

bool _readBool(Map<String, dynamic> json, String key, {bool fallback = false}) {
  final value = json[key];
  return value is bool ? value : fallback;
}

T _readEnum<T>(
  Map<String, dynamic> json,
  String key,
  List<T> values,
  T fallback,
) {
  final index = _readInt(json, key, -1);
  if (index < 0 || index >= values.length) {
    return fallback;
  }
  return values[index];
}

class JobSnapshot {
  const JobSnapshot({
    this.mediaInfo,
    this.progress,
    this.logs = const <LogEntry>[],
    this.errorMessage,
    this.jobId,
    this.isBusy = false,
  });

  final MediaInfoModel? mediaInfo;
  final JobProgressModel? progress;
  final List<LogEntry> logs;
  final String? errorMessage;
  final int? jobId;
  final bool isBusy;

  JobSnapshot copyWith({
    MediaInfoModel? mediaInfo,
    JobProgressModel? progress,
    List<LogEntry>? logs,
    String? errorMessage,
    int? jobId,
    bool? isBusy,
    bool clearError = false,
  }) {
    return JobSnapshot(
      mediaInfo: mediaInfo ?? this.mediaInfo,
      progress: progress ?? this.progress,
      logs: logs ?? this.logs,
      errorMessage: clearError ? null : errorMessage ?? this.errorMessage,
      jobId: jobId ?? this.jobId,
      isBusy: isBusy ?? this.isBusy,
    );
  }
}
