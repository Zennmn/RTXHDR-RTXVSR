import 'package:flutter/material.dart';

import '../domain/system_probe_model.dart';

class SystemCapabilityPanel extends StatelessWidget {
  const SystemCapabilityPanel({
    required this.probe,
    required this.isLoading,
    required this.errorMessage,
    super.key,
  });

  final SystemProbeModel? probe;
  final bool isLoading;
  final String? errorMessage;

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
              Text('系统能力', style: theme.textTheme.titleLarge),
              const SizedBox(height: 16),
              if (isLoading) const LinearProgressIndicator(),
              if (errorMessage != null)
                Text(
                  errorMessage!,
                  style: TextStyle(color: theme.colorScheme.error),
                ),
              if (probe != null) ...[
                _FactRow(label: '系统', value: probe!.osVersion),
                _FactRow(label: '显卡', value: probe!.gpuName),
                _FactRow(label: '驱动', value: probe!.driverVersion),
                _FactRow(label: 'CUDA', value: probe!.cudaVersion),
                const SizedBox(height: 14),
                Wrap(
                  spacing: 8,
                  runSpacing: 8,
                  children: [
                    _CapabilityChip(
                      label: 'NGX VSR',
                      enabled: probe!.ngxVsrAvailable,
                    ),
                    _CapabilityChip(
                      label: 'TrueHDR',
                      enabled: probe!.ngxTruehdrAvailable,
                    ),
                    _CapabilityChip(
                      label: 'HDR Export',
                      enabled: probe!.hdrExportAvailable,
                    ),
                    _CapabilityChip(
                      label: 'H.264 Decode',
                      enabled: probe!.h264HwDecode,
                    ),
                    _CapabilityChip(
                      label: 'HEVC Decode',
                      enabled: probe!.hevcHwDecode,
                    ),
                    _CapabilityChip(
                      label: 'AV1 Decode',
                      enabled: probe!.av1HwDecode,
                    ),
                    _CapabilityChip(
                      label: 'H.264 Encode',
                      enabled: probe!.h264HwEncode,
                    ),
                    _CapabilityChip(
                      label: 'HEVC Encode',
                      enabled: probe!.hevcHwEncode,
                    ),
                    _CapabilityChip(
                      label: 'AV1 Encode',
                      enabled: probe!.av1HwEncode,
                    ),
                  ],
                ),
                const SizedBox(height: 16),
                Text('支持像素格式: ${probe!.supportedPixelFormats}'),
                if (probe!.notes.isNotEmpty) ...[
                  const SizedBox(height: 8),
                  Text('备注: ${probe!.notes}'),
                ],
              ],
            ],
          ),
        ),
      ),
    );
  }
}

class _FactRow extends StatelessWidget {
  const _FactRow({required this.label, required this.value});

  final String label;
  final String value;

  @override
  Widget build(BuildContext context) {
    return Padding(
      padding: const EdgeInsets.only(bottom: 8),
      child: Row(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          SizedBox(
            width: 68,
            child: Text(
              label,
              style: const TextStyle(fontWeight: FontWeight.w700),
            ),
          ),
          Expanded(child: Text(value.isEmpty ? '-' : value)),
        ],
      ),
    );
  }
}

class _CapabilityChip extends StatelessWidget {
  const _CapabilityChip({required this.label, required this.enabled});

  final String label;
  final bool enabled;

  @override
  Widget build(BuildContext context) {
    final colorScheme = Theme.of(context).colorScheme;
    return Chip(
      label: Text(label),
      backgroundColor: enabled
          ? colorScheme.primaryContainer
          : colorScheme.surfaceContainerHighest,
      side: BorderSide.none,
    );
  }
}
