import 'package:flutter/material.dart';

import '../domain/job_models.dart';

class JobFormPanel extends StatelessWidget {
  const JobFormPanel({
    required this.draft,
    required this.inputController,
    required this.outputController,
    required this.customWidthController,
    required this.customHeightController,
    required this.onInputBrowse,
    required this.onOutputBrowse,
    required this.onDraftChanged,
    required this.onAnalyze,
    required this.onStart,
    required this.onCancel,
    required this.isBusy,
    required this.hasActiveJob,
    super.key,
  });

  final JobConfigDraft draft;
  final TextEditingController inputController;
  final TextEditingController outputController;
  final TextEditingController customWidthController;
  final TextEditingController customHeightController;
  final VoidCallback onInputBrowse;
  final VoidCallback onOutputBrowse;
  final ValueChanged<JobConfigDraft> onDraftChanged;
  final VoidCallback onAnalyze;
  final VoidCallback onStart;
  final VoidCallback onCancel;
  final bool isBusy;
  final bool hasActiveJob;

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    return Card(
      child: Padding(
        padding: const EdgeInsets.all(20),
        child: SingleChildScrollView(
          child: Column(
            crossAxisAlignment: CrossAxisAlignment.start,
            children: [
              Text('导出设置', style: theme.textTheme.titleLarge),
              const SizedBox(height: 6),
              Text(
                '当前设置会在本机自动保存，并在下次启动时恢复。',
                style: theme.textTheme.bodyMedium,
              ),
              const SizedBox(height: 16),
              _PathField(
                label: '输入视频',
                controller: inputController,
                buttonLabel: '浏览',
                onPressed: onInputBrowse,
                onChanged: (value) =>
                    onDraftChanged(draft.copyWith(inputPath: value)),
              ),
              const SizedBox(height: 12),
              _PathField(
                label: '输出文件',
                controller: outputController,
                buttonLabel: '另存为',
                onPressed: onOutputBrowse,
                onChanged: (value) =>
                    onDraftChanged(draft.copyWith(outputPath: value)),
              ),
              const SizedBox(height: 12),
              Row(
                children: [
                  Expanded(
                    child: DropdownButtonFormField<OutputContainer>(
                      initialValue: draft.container,
                      items: const [
                        DropdownMenuItem(
                          value: OutputContainer.mkv,
                          child: Text('MKV'),
                        ),
                        DropdownMenuItem(
                          value: OutputContainer.mp4,
                          child: Text('MP4'),
                        ),
                      ],
                      onChanged: (value) {
                        if (value != null) {
                          onDraftChanged(draft.copyWith(container: value));
                        }
                      },
                      decoration: const InputDecoration(labelText: '封装格式'),
                    ),
                  ),
                  const SizedBox(width: 12),
                  Expanded(
                    child: DropdownButtonFormField<OutputVideoCodec>(
                      initialValue: draft.videoCodec,
                      items: const [
                        DropdownMenuItem(
                          value: OutputVideoCodec.hevc,
                          child: Text('HEVC Main10'),
                        ),
                        DropdownMenuItem(
                          value: OutputVideoCodec.av1,
                          child: Text('AV1 10-bit'),
                        ),
                      ],
                      onChanged: (value) {
                        if (value != null) {
                          onDraftChanged(draft.copyWith(videoCodec: value));
                        }
                      },
                      decoration: const InputDecoration(labelText: '视频编码'),
                    ),
                  ),
                ],
              ),
              const SizedBox(height: 12),
              Row(
                children: [
                  Expanded(
                    child: DropdownButtonFormField<OutputAudioMode>(
                      initialValue: draft.audioMode,
                      items: const [
                        DropdownMenuItem(
                          value: OutputAudioMode.copyPreferred,
                          child: Text('Copy Audio'),
                        ),
                        DropdownMenuItem(
                          value: OutputAudioMode.aac,
                          child: Text('AAC'),
                        ),
                        DropdownMenuItem(
                          value: OutputAudioMode.disabled,
                          child: Text('None'),
                        ),
                      ],
                      onChanged: (value) {
                        if (value != null) {
                          onDraftChanged(draft.copyWith(audioMode: value));
                        }
                      },
                      decoration: const InputDecoration(labelText: '音频'),
                    ),
                  ),
                  const SizedBox(width: 12),
                  Expanded(
                    child: DropdownButtonFormField<QualityPreset>(
                      initialValue: draft.qualityPreset,
                      items: const [
                        DropdownMenuItem(
                          value: QualityPreset.fast,
                          child: Text('Fast'),
                        ),
                        DropdownMenuItem(
                          value: QualityPreset.balanced,
                          child: Text('Balanced'),
                        ),
                        DropdownMenuItem(
                          value: QualityPreset.highQuality,
                          child: Text('High Quality'),
                        ),
                      ],
                      onChanged: (value) {
                        if (value != null) {
                          onDraftChanged(draft.copyWith(qualityPreset: value));
                        }
                      },
                      decoration: const InputDecoration(labelText: '画质预设'),
                    ),
                  ),
                ],
              ),
              const SizedBox(height: 12),
              DropdownButtonFormField<VideoRateControlMode>(
                initialValue: draft.videoRateControlMode,
                items: const [
                  DropdownMenuItem(
                    value: VideoRateControlMode.auto,
                    child: Text('Auto Rate Control'),
                  ),
                  DropdownMenuItem(
                    value: VideoRateControlMode.targetBitrate,
                    child: Text('Target Bitrate'),
                  ),
                  DropdownMenuItem(
                    value: VideoRateControlMode.constantQuality,
                    child: Text('Constant Quality'),
                  ),
                ],
                onChanged: (value) {
                  if (value != null) {
                    onDraftChanged(draft.copyWith(videoRateControlMode: value));
                  }
                },
                decoration: const InputDecoration(labelText: '视频码率控制'),
              ),
              if (draft.videoRateControlMode ==
                  VideoRateControlMode.targetBitrate) ...[
                const SizedBox(height: 8),
                _SliderSetting(
                  label: 'Target Bitrate',
                  value: draft.targetVideoBitrateMbps.toDouble(),
                  min: 1,
                  max: 80,
                  divisions: 79,
                  displayValue: '${draft.targetVideoBitrateMbps} Mbps',
                  onChanged: (value) => onDraftChanged(
                    draft.copyWith(targetVideoBitrateMbps: value.round()),
                  ),
                ),
              ],
              if (draft.videoRateControlMode ==
                  VideoRateControlMode.constantQuality) ...[
                const SizedBox(height: 8),
                _SliderSetting(
                  label: 'Constant Quality',
                  value: draft.constantQuality.toDouble(),
                  min: 0,
                  max: 35,
                  divisions: 35,
                  displayValue: draft.constantQuality.toString(),
                  onChanged: (value) => onDraftChanged(
                    draft.copyWith(constantQuality: value.round()),
                  ),
                ),
              ],
              const SizedBox(height: 18),
              SegmentedButton<ResolutionMode>(
                segments: const [
                  ButtonSegment(
                    value: ResolutionMode.keep,
                    label: Text('保持原分辨率'),
                  ),
                  ButtonSegment(
                    value: ResolutionMode.doubleSize,
                    label: Text('2 倍超分'),
                  ),
                  ButtonSegment(
                    value: ResolutionMode.custom,
                    label: Text('自定义'),
                  ),
                ],
                selected: {draft.resolutionMode},
                onSelectionChanged: (selection) {
                  onDraftChanged(
                    draft.copyWith(resolutionMode: selection.first),
                  );
                },
              ),
              const SizedBox(height: 12),
              if (draft.resolutionMode == ResolutionMode.custom)
                Row(
                  children: [
                    Expanded(
                      child: TextField(
                        controller: customWidthController,
                        keyboardType: TextInputType.number,
                        decoration: const InputDecoration(labelText: '自定义宽度'),
                        onChanged: (value) =>
                            onDraftChanged(draft.copyWith(customWidth: value)),
                      ),
                    ),
                    const SizedBox(width: 12),
                    Expanded(
                      child: TextField(
                        controller: customHeightController,
                        keyboardType: TextInputType.number,
                        decoration: const InputDecoration(labelText: '自定义高度'),
                        onChanged: (value) =>
                            onDraftChanged(draft.copyWith(customHeight: value)),
                      ),
                    ),
                  ],
                ),
              const SizedBox(height: 10),
              SwitchListTile(
                value: draft.hdrEnabled,
                contentPadding: EdgeInsets.zero,
                title: const Text('启用 SDR 转 HDR'),
                subtitle: const Text('以 10-bit HDR 元数据链路导出'),
                onChanged: (value) =>
                    onDraftChanged(draft.copyWith(hdrEnabled: value)),
              ),
              if (draft.hdrEnabled) ...[
                const SizedBox(height: 8),
                Text('RTX Video HDR 参数', style: theme.textTheme.titleMedium),
                const SizedBox(height: 8),
                _SliderSetting(
                  label: 'Contrast',
                  value: draft.trueHdrContrast.toDouble(),
                  min: 0,
                  max: 200,
                  divisions: 200,
                  displayValue: draft.trueHdrContrast.toString(),
                  onChanged: (value) => onDraftChanged(
                    draft.copyWith(trueHdrContrast: value.round()),
                  ),
                ),
                _SliderSetting(
                  label: 'Saturation',
                  value: draft.trueHdrSaturation.toDouble(),
                  min: 0,
                  max: 200,
                  divisions: 200,
                  displayValue: draft.trueHdrSaturation.toString(),
                  onChanged: (value) => onDraftChanged(
                    draft.copyWith(trueHdrSaturation: value.round()),
                  ),
                ),
                _SliderSetting(
                  label: 'Middle Gray',
                  value: draft.trueHdrMiddleGray.toDouble(),
                  min: 10,
                  max: 100,
                  divisions: 90,
                  displayValue: draft.trueHdrMiddleGray.toString(),
                  onChanged: (value) => onDraftChanged(
                    draft.copyWith(trueHdrMiddleGray: value.round()),
                  ),
                ),
                _SliderSetting(
                  label: 'Max Luminance',
                  value: draft.trueHdrMaxLuminance.toDouble(),
                  min: 400,
                  max: 2000,
                  divisions: 32,
                  displayValue: '${draft.trueHdrMaxLuminance} nits',
                  onChanged: (value) => onDraftChanged(
                    draft.copyWith(trueHdrMaxLuminance: value.round()),
                  ),
                ),
              ],
              SwitchListTile(
                value: draft.keepAudio,
                contentPadding: EdgeInsets.zero,
                title: const Text('保留音频'),
                subtitle: const Text('优先直通，必要时再重新编码'),
                onChanged: (value) =>
                    onDraftChanged(draft.copyWith(keepAudio: value)),
              ),
              SwitchListTile(
                value: draft.overwriteOutput,
                contentPadding: EdgeInsets.zero,
                title: const Text('允许覆盖'),
                subtitle: const Text('默认情况下不会覆盖已存在的文件'),
                onChanged: (value) =>
                    onDraftChanged(draft.copyWith(overwriteOutput: value)),
              ),
              const SizedBox(height: 12),
              Row(
                children: [
                  FilledButton.tonalIcon(
                    onPressed: isBusy ? null : onAnalyze,
                    icon: const Icon(Icons.analytics_outlined),
                    label: const Text('分析'),
                  ),
                  const SizedBox(width: 12),
                  FilledButton.icon(
                    onPressed: isBusy || hasActiveJob ? null : onStart,
                    icon: const Icon(Icons.play_arrow_rounded),
                    label: const Text('开始导出'),
                  ),
                  const SizedBox(width: 12),
                  OutlinedButton.icon(
                    onPressed: hasActiveJob ? onCancel : null,
                    icon: const Icon(Icons.stop_circle_outlined),
                    label: const Text('取消'),
                  ),
                ],
              ),
            ],
          ),
        ),
      ),
    );
  }
}

class _PathField extends StatelessWidget {
  const _PathField({
    required this.label,
    required this.controller,
    required this.buttonLabel,
    required this.onPressed,
    required this.onChanged,
  });

  final String label;
  final TextEditingController controller;
  final String buttonLabel;
  final VoidCallback onPressed;
  final ValueChanged<String> onChanged;

  @override
  Widget build(BuildContext context) {
    return Row(
      crossAxisAlignment: CrossAxisAlignment.end,
      children: [
        Expanded(
          child: TextField(
            controller: controller,
            decoration: InputDecoration(labelText: label),
            onChanged: onChanged,
          ),
        ),
        const SizedBox(width: 12),
        FilledButton.tonal(onPressed: onPressed, child: Text(buttonLabel)),
      ],
    );
  }
}

class _SliderSetting extends StatelessWidget {
  const _SliderSetting({
    required this.label,
    required this.value,
    required this.min,
    required this.max,
    required this.divisions,
    required this.displayValue,
    required this.onChanged,
  });

  final String label;
  final double value;
  final double min;
  final double max;
  final int divisions;
  final String displayValue;
  final ValueChanged<double> onChanged;

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    return Padding(
      padding: const EdgeInsets.only(bottom: 4),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          Row(
            children: [
              Expanded(child: Text(label, style: theme.textTheme.labelLarge)),
              Text(displayValue, style: theme.textTheme.bodyMedium),
            ],
          ),
          Slider(
            value: value.clamp(min, max),
            min: min,
            max: max,
            divisions: divisions,
            label: displayValue,
            onChanged: onChanged,
          ),
        ],
      ),
    );
  }
}
