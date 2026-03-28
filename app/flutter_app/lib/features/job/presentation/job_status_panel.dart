import 'package:flutter/material.dart';

import '../../../core/logging/log_entry.dart';
import '../domain/job_models.dart';

class JobStatusPanel extends StatelessWidget {
  const JobStatusPanel({
    required this.mediaInfo,
    required this.progress,
    required this.logs,
    required this.errorMessage,
    super.key,
  });

  final MediaInfoModel? mediaInfo;
  final JobProgressModel? progress;
  final List<LogEntry> logs;
  final String? errorMessage;

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    final colorScheme = theme.colorScheme;

    return Card(
      child: Padding(
        padding: const EdgeInsets.all(20),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            Text('任务面板', style: theme.textTheme.titleLarge),
            const SizedBox(height: 16),
            if (mediaInfo != null) ...[
              _MetricRow(
                label: '分辨率',
                value: '${mediaInfo!.width} x ${mediaInfo!.height}',
              ),
              _MetricRow(label: '视频', value: mediaInfo!.videoCodec),
              _MetricRow(
                label: '色彩',
                value: '${mediaInfo!.colorPrimaries} / ${mediaInfo!.transfer}',
              ),
              _MetricRow(label: '位深', value: '${mediaInfo!.bitDepth}-bit'),
              const SizedBox(height: 12),
            ],
            if (progress != null) ...[
              Text(
                localizeRuntimeText(progress!.currentPhase),
                style: theme.textTheme.titleMedium,
              ),
              const SizedBox(height: 8),
              LinearProgressIndicator(
                value: progress!.totalProgress.clamp(0, 1),
              ),
              const SizedBox(height: 10),
              _MetricRow(label: '状态', value: progress!.state.localizedLabel),
              _MetricRow(label: 'FPS', value: progress!.fps.toStringAsFixed(2)),
              _MetricRow(
                label: '帧数',
                value:
                    '${progress!.processedFrames} / ${progress!.totalFrames}',
              ),
              _MetricRow(
                label: '耗时',
                value: '${progress!.elapsedSeconds.toStringAsFixed(1)} 秒',
              ),
              _MetricRow(
                label: '预计剩余',
                value: progress!.etaSeconds.isNegative
                    ? '-'
                    : '${progress!.etaSeconds.toStringAsFixed(1)} 秒',
              ),
              const SizedBox(height: 8),
              Text(localizeRuntimeText(progress!.statusMessage)),
            ],
            if (errorMessage != null) ...[
              const SizedBox(height: 12),
              Text(errorMessage!, style: TextStyle(color: colorScheme.error)),
            ],
            const SizedBox(height: 16),
            Expanded(
              child: DecoratedBox(
                decoration: BoxDecoration(
                  color: colorScheme.surfaceContainerHighest.withValues(
                    alpha: 0.55,
                  ),
                  borderRadius: BorderRadius.circular(18),
                ),
                child: Padding(
                  padding: const EdgeInsets.all(14),
                  child: ListView.separated(
                    itemCount: logs.length,
                    itemBuilder: (context, index) {
                      final entry = logs[index];
                      return SelectableText(
                        '[${entry.localizedLevel}] '
                        '${localizeRuntimeText(entry.phase)}: '
                        '${localizeRuntimeText(entry.message)}',
                        style: const TextStyle(
                          fontFamily: 'Cascadia Mono',
                          fontSize: 12,
                        ),
                      );
                    },
                    separatorBuilder: (_, _) => const SizedBox(height: 6),
                  ),
                ),
              ),
            ),
          ],
        ),
      ),
    );
  }
}

class _MetricRow extends StatelessWidget {
  const _MetricRow({required this.label, required this.value});

  final String label;
  final String value;

  @override
  Widget build(BuildContext context) {
    return Padding(
      padding: const EdgeInsets.only(bottom: 6),
      child: Row(
        children: [
          SizedBox(
            width: 84,
            child: Text(
              label,
              style: const TextStyle(fontWeight: FontWeight.w700),
            ),
          ),
          Expanded(child: Text(value)),
        ],
      ),
    );
  }
}
