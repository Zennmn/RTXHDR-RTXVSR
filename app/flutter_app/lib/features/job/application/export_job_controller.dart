import 'dart:async';
import 'dart:io';
import 'dart:isolate';

import 'package:flutter/foundation.dart';
import 'package:path/path.dart' as p;

import '../../../core/ffi/native_library.dart';
import '../../../core/logging/log_entry.dart';
import '../data/job_draft_store.dart';
import '../domain/job_models.dart';

class ExportJobController extends ChangeNotifier {
  ExportJobController({JobDraftStore? draftStore})
    : _draftStore = draftStore ?? const JobDraftStore();

  NativeLibrary get _native => NativeLibrary.instance;
  final JobDraftStore _draftStore;
  Timer? _pollTimer;
  Timer? _saveDraftTimer;
  int _analysisToken = 0;

  JobConfigDraft draft = const JobConfigDraft();
  JobSnapshot snapshot = const JobSnapshot();

  void updateDraft(JobConfigDraft next) {
    draft = next;
    _scheduleDraftSave();
    notifyListeners();
  }

  Future<void> restoreLastDraft() async {
    final restoredDraft = await _draftStore.load();
    if (restoredDraft == null) {
      return;
    }

    draft = restoredDraft;
    notifyListeners();

    if (draft.inputPath.isEmpty || !File(draft.inputPath).existsSync()) {
      return;
    }

    await analyzeInput();
  }

  Future<void> analyzeInput() async {
    if (draft.inputPath.isEmpty) {
      return;
    }

    final requestedPath = draft.inputPath;
    final token = ++_analysisToken;

    snapshot = snapshot.copyWith(isBusy: true, clearError: true);
    notifyListeners();

    try {
      await Future<void>.delayed(Duration.zero);
      final mediaInfo = await _analyzeInputInBackground(requestedPath);
      if (token != _analysisToken || requestedPath != draft.inputPath) {
        return;
      }
      final suggestedOutput = draft.outputPath.isEmpty
          ? _defaultOutputPath(requestedPath)
          : draft.outputPath;
      draft = draft.copyWith(outputPath: suggestedOutput);
      _scheduleDraftSave();
      snapshot = snapshot.copyWith(
        mediaInfo: mediaInfo,
        isBusy: false,
        clearError: true,
      );
    } catch (error) {
      if (token != _analysisToken || requestedPath != draft.inputPath) {
        return;
      }
      snapshot = snapshot.copyWith(
        isBusy: false,
        errorMessage: error.toString(),
      );
    }

    notifyListeners();
  }

  Future<void> startJob() async {
    snapshot = snapshot.copyWith(isBusy: true, clearError: true);
    notifyListeners();

    try {
      await _persistDraftNow();
      await Future<void>.delayed(Duration.zero);
      final jobId = await _createAndStartJobInBackground(
        draft: draft,
        mediaInfo: snapshot.mediaInfo,
      );
      snapshot = snapshot.copyWith(
        jobId: jobId,
        logs: const <LogEntry>[],
        isBusy: false,
        clearError: true,
      );
      _startPolling(jobId);
    } catch (error) {
      snapshot = snapshot.copyWith(
        isBusy: false,
        errorMessage: error.toString(),
      );
    }

    notifyListeners();
  }

  Future<int> _createAndStartJobInBackground({
    required JobConfigDraft draft,
    required MediaInfoModel? mediaInfo,
  }) async {
    final request = <String, Object?>{
      'inputPath': draft.inputPath,
      'outputPath': draft.outputPath,
      'container': draft.container.index,
      'videoCodec': draft.videoCodec.index,
      'audioMode': draft.audioMode.index,
      'qualityPreset': draft.qualityPreset.index,
      'resolutionMode': draft.resolutionMode.index,
      'customWidth': draft.customWidth,
      'customHeight': draft.customHeight,
      'hdrEnabled': draft.hdrEnabled,
      'trueHdrContrast': draft.trueHdrContrast,
      'trueHdrSaturation': draft.trueHdrSaturation,
      'trueHdrMiddleGray': draft.trueHdrMiddleGray,
      'trueHdrMaxLuminance': draft.trueHdrMaxLuminance,
      'videoRateControlMode': draft.videoRateControlMode.index,
      'targetVideoBitrateMbps': draft.targetVideoBitrateMbps,
      'constantQuality': draft.constantQuality,
      'keepAudio': draft.keepAudio,
      'overwriteOutput': draft.overwriteOutput,
      'lockAspectRatio': draft.lockAspectRatio,
      'gpuIndex': draft.gpuIndex,
      'mediaWidth': mediaInfo?.width ?? 0,
      'mediaHeight': mediaInfo?.height ?? 0,
    };

    return Isolate.run(() {
      final backgroundDraft = JobConfigDraft(
        inputPath: request['inputPath']! as String,
        outputPath: request['outputPath']! as String,
        container: OutputContainer.values[request['container']! as int],
        videoCodec: OutputVideoCodec.values[request['videoCodec']! as int],
        audioMode: OutputAudioMode.values[request['audioMode']! as int],
        qualityPreset: QualityPreset.values[request['qualityPreset']! as int],
        resolutionMode:
            ResolutionMode.values[request['resolutionMode']! as int],
        customWidth: request['customWidth']! as String,
        customHeight: request['customHeight']! as String,
        hdrEnabled: request['hdrEnabled']! as bool,
        trueHdrContrast: request['trueHdrContrast']! as int,
        trueHdrSaturation: request['trueHdrSaturation']! as int,
        trueHdrMiddleGray: request['trueHdrMiddleGray']! as int,
        trueHdrMaxLuminance: request['trueHdrMaxLuminance']! as int,
        videoRateControlMode: VideoRateControlMode
            .values[request['videoRateControlMode']! as int],
        targetVideoBitrateMbps: request['targetVideoBitrateMbps']! as int,
        constantQuality: request['constantQuality']! as int,
        keepAudio: request['keepAudio']! as bool,
        overwriteOutput: request['overwriteOutput']! as bool,
        lockAspectRatio: request['lockAspectRatio']! as bool,
        gpuIndex: request['gpuIndex']! as int,
      );

      MediaInfoModel? backgroundMediaInfo;
      final mediaWidth = request['mediaWidth']! as int;
      final mediaHeight = request['mediaHeight']! as int;
      if (mediaWidth > 0 && mediaHeight > 0) {
        backgroundMediaInfo = MediaInfoModel(
          containerName: '',
          videoCodec: '',
          audioCodec: '',
          pixelFormat: '',
          colorPrimaries: '',
          transfer: '',
          matrix: '',
          colorRange: '',
          width: mediaWidth,
          height: mediaHeight,
          frameRate: 0,
          durationSeconds: 0,
          frameCount: 0,
          bitDepth: 8,
          audioChannels: 0,
          audioSampleRate: 0,
          hasAudio: false,
          hdrSignaled: false,
        );
      }

      final native = NativeLibrary.instance;
      final jobId = native.createJob(
        backgroundDraft,
        mediaInfo: backgroundMediaInfo,
      );
      native.startJob(jobId);
      return jobId;
    });
  }

  Future<void> cancelJob() async {
    final jobId = snapshot.jobId;
    if (jobId == null) {
      return;
    }

    try {
      _native.cancelJob(jobId);
    } catch (error) {
      snapshot = snapshot.copyWith(errorMessage: error.toString());
      notifyListeners();
    }
  }

  void _scheduleDraftSave() {
    _saveDraftTimer?.cancel();
    _saveDraftTimer = Timer(
      const Duration(milliseconds: 250),
      () => unawaited(_persistDraftNow()),
    );
  }

  Future<void> _persistDraftNow() async {
    _saveDraftTimer?.cancel();
    await _draftStore.save(draft);
  }

  void _startPolling(int jobId) {
    _pollTimer?.cancel();
    _pollTimer = Timer.periodic(
      const Duration(milliseconds: 400),
      (_) => _poll(jobId),
    );
    unawaited(_poll(jobId));
  }

  Future<void> _poll(int jobId) async {
    try {
      final progress = _native.queryProgress(jobId);
      final newLogs = _native.drainEvents(jobId);
      snapshot = snapshot.copyWith(
        progress: progress,
        logs: <LogEntry>[...snapshot.logs, ...newLogs],
      );

      if (progress.isTerminal) {
        _pollTimer?.cancel();
      }
    } catch (error) {
      _pollTimer?.cancel();
      snapshot = snapshot.copyWith(errorMessage: error.toString());
    }

    notifyListeners();
  }

  Future<MediaInfoModel> _analyzeInputInBackground(String inputPath) async {
    final data = await Isolate.run(() {
      final mediaInfo = NativeLibrary.instance.analyzeInput(inputPath);
      return <String, Object?>{
        'containerName': mediaInfo.containerName,
        'videoCodec': mediaInfo.videoCodec,
        'audioCodec': mediaInfo.audioCodec,
        'pixelFormat': mediaInfo.pixelFormat,
        'colorPrimaries': mediaInfo.colorPrimaries,
        'transfer': mediaInfo.transfer,
        'matrix': mediaInfo.matrix,
        'colorRange': mediaInfo.colorRange,
        'width': mediaInfo.width,
        'height': mediaInfo.height,
        'frameRate': mediaInfo.frameRate,
        'durationSeconds': mediaInfo.durationSeconds,
        'frameCount': mediaInfo.frameCount,
        'bitDepth': mediaInfo.bitDepth,
        'audioChannels': mediaInfo.audioChannels,
        'audioSampleRate': mediaInfo.audioSampleRate,
        'hasAudio': mediaInfo.hasAudio,
        'hdrSignaled': mediaInfo.hdrSignaled,
      };
    });

    return MediaInfoModel(
      containerName: data['containerName']! as String,
      videoCodec: data['videoCodec']! as String,
      audioCodec: data['audioCodec']! as String,
      pixelFormat: data['pixelFormat']! as String,
      colorPrimaries: data['colorPrimaries']! as String,
      transfer: data['transfer']! as String,
      matrix: data['matrix']! as String,
      colorRange: data['colorRange']! as String,
      width: data['width']! as int,
      height: data['height']! as int,
      frameRate: data['frameRate']! as double,
      durationSeconds: data['durationSeconds']! as double,
      frameCount: data['frameCount']! as int,
      bitDepth: data['bitDepth']! as int,
      audioChannels: data['audioChannels']! as int,
      audioSampleRate: data['audioSampleRate']! as int,
      hasAudio: data['hasAudio']! as bool,
      hdrSignaled: data['hdrSignaled']! as bool,
    );
  }

  String _defaultOutputPath(String inputPath) {
    final directory = p.dirname(inputPath);
    final stem = p.basenameWithoutExtension(inputPath);
    final extension = draft.container == OutputContainer.mp4 ? '.mp4' : '.mkv';
    return p.join(directory, '${stem}_enhanced$extension');
  }

  @override
  void dispose() {
    _pollTimer?.cancel();
    _saveDraftTimer?.cancel();
    unawaited(_persistDraftNow());
    final jobId = snapshot.jobId;
    if (jobId != null) {
      try {
        _native.destroyJob(jobId);
      } on Object {
        // Ignore cleanup failures on shutdown.
      }
    }
    super.dispose();
  }
}
