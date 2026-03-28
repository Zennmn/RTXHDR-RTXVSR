import 'dart:io';

import 'package:flutter/material.dart';
import 'package:path/path.dart' as p;

import '../../system/application/system_probe_controller.dart';
import '../../system/presentation/system_capability_panel.dart';
import '../application/export_job_controller.dart';
import '../domain/job_models.dart';
import 'job_form_panel.dart';
import 'job_status_panel.dart';

class ExportDashboardPage extends StatefulWidget {
  const ExportDashboardPage({
    required this.systemProbeController,
    required this.exportJobController,
    super.key,
  });

  final SystemProbeController systemProbeController;
  final ExportJobController exportJobController;

  @override
  State<ExportDashboardPage> createState() => _ExportDashboardPageState();
}

class _ExportDashboardPageState extends State<ExportDashboardPage> {
  late final TextEditingController _inputController;
  late final TextEditingController _outputController;
  late final TextEditingController _customWidthController;
  late final TextEditingController _customHeightController;

  @override
  void initState() {
    super.initState();
    _inputController = TextEditingController();
    _outputController = TextEditingController();
    _customWidthController = TextEditingController();
    _customHeightController = TextEditingController();
    widget.exportJobController.addListener(_syncControllers);
  }

  @override
  void dispose() {
    widget.exportJobController.removeListener(_syncControllers);
    _inputController.dispose();
    _outputController.dispose();
    _customWidthController.dispose();
    _customHeightController.dispose();
    super.dispose();
  }

  void _syncControllers() {
    final draft = widget.exportJobController.draft;
    if (_inputController.text != draft.inputPath) {
      _inputController.text = draft.inputPath;
    }
    if (_outputController.text != draft.outputPath) {
      _outputController.text = draft.outputPath;
    }
    if (_customWidthController.text != draft.customWidth) {
      _customWidthController.text = draft.customWidth;
    }
    if (_customHeightController.text != draft.customHeight) {
      _customHeightController.text = draft.customHeight;
    }
  }

  Future<void> _pickInput() async {
    final selectedPath = await _showOpenFileDialog();
    if (selectedPath == null) {
      return;
    }

    final currentDraft = widget.exportJobController.draft;
    final extension = currentDraft.container == OutputContainer.mp4
        ? '.mp4'
        : '.mkv';
    final suggestedOutput = p.join(
      p.dirname(selectedPath),
      '${p.basenameWithoutExtension(selectedPath)}_enhanced$extension',
    );

    widget.exportJobController.updateDraft(
      currentDraft.copyWith(
        inputPath: selectedPath,
        outputPath: suggestedOutput,
      ),
    );
    await widget.exportJobController.analyzeInput();
  }

  Future<void> _pickOutput() async {
    final extension =
        widget.exportJobController.draft.container == OutputContainer.mp4
        ? 'mp4'
        : 'mkv';
    final selectedPath = await _showSaveFileDialog(
      'enhanced_output.$extension',
    );
    if (selectedPath == null) {
      return;
    }

    widget.exportJobController.updateDraft(
      widget.exportJobController.draft.copyWith(outputPath: selectedPath),
    );
  }

  Future<String?> _showOpenFileDialog() async {
    const script = r'''
Add-Type -AssemblyName System.Windows.Forms
$dialog = New-Object System.Windows.Forms.OpenFileDialog
$dialog.Filter = '视频文件|*.mp4;*.mkv;*.mov'
$dialog.Multiselect = $false
if ($dialog.ShowDialog() -eq [System.Windows.Forms.DialogResult]::OK) { Write-Output $dialog.FileName }
''';
    final result = await Process.run('powershell', [
      '-STA',
      '-NoProfile',
      '-Command',
      script,
    ]);
    final output = (result.stdout as String).trim();
    return output.isEmpty ? null : output;
  }

  Future<String?> _showSaveFileDialog(String suggestedName) async {
    final script =
        '''
Add-Type -AssemblyName System.Windows.Forms
\$dialog = New-Object System.Windows.Forms.SaveFileDialog
\$dialog.Filter = '视频文件|*.mp4;*.mkv'
\$dialog.FileName = '$suggestedName'
if (\$dialog.ShowDialog() -eq [System.Windows.Forms.DialogResult]::OK) { Write-Output \$dialog.FileName }
''';
    final result = await Process.run('powershell', [
      '-STA',
      '-NoProfile',
      '-Command',
      script,
    ]);
    final output = (result.stdout as String).trim();
    return output.isEmpty ? null : output;
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      body: Container(
        decoration: const BoxDecoration(
          gradient: LinearGradient(
            begin: Alignment.topLeft,
            end: Alignment.bottomRight,
            colors: [Color(0xFFF4EFE6), Color(0xFFE9E5DD), Color(0xFFDDE4E2)],
          ),
        ),
        child: SafeArea(
          child: Padding(
            padding: const EdgeInsets.all(20),
            child: ListenableBuilder(
              listenable: Listenable.merge([
                widget.systemProbeController,
                widget.exportJobController,
              ]),
              builder: (context, _) {
                final jobSnapshot = widget.exportJobController.snapshot;
                return Column(
                  crossAxisAlignment: CrossAxisAlignment.start,
                  children: [
                    Text(
                      'RTXHDR + RTXVSR 视频增强工作台',
                      style: Theme.of(context).textTheme.headlineMedium,
                    ),
                    const SizedBox(height: 6),
                    const Text(
                      '面向 NVIDIA RTX 平台的离线视频增强工具，集成原生 FFmpeg 与 CUDA/NGX 处理管线。',
                    ),
                    const SizedBox(height: 20),
                    Expanded(
                      child: LayoutBuilder(
                        builder: (context, constraints) {
                          final hasActiveJob =
                              jobSnapshot.jobId != null &&
                              !(jobSnapshot.progress?.isTerminal ?? false);

                          final formPanel = JobFormPanel(
                            draft: widget.exportJobController.draft,
                            inputController: _inputController,
                            outputController: _outputController,
                            customWidthController: _customWidthController,
                            customHeightController: _customHeightController,
                            onInputBrowse: _pickInput,
                            onOutputBrowse: _pickOutput,
                            onDraftChanged:
                                widget.exportJobController.updateDraft,
                            onAnalyze: widget.exportJobController.analyzeInput,
                            onStart: widget.exportJobController.startJob,
                            onCancel: widget.exportJobController.cancelJob,
                            isBusy: jobSnapshot.isBusy,
                            hasActiveJob: hasActiveJob,
                          );

                          final systemPanel = SystemCapabilityPanel(
                            probe: widget.systemProbeController.probe,
                            isLoading: widget.systemProbeController.isLoading,
                            errorMessage:
                                widget.systemProbeController.errorMessage,
                          );

                          final statusPanel = JobStatusPanel(
                            mediaInfo: jobSnapshot.mediaInfo,
                            progress: jobSnapshot.progress,
                            logs: jobSnapshot.logs,
                            errorMessage: jobSnapshot.errorMessage,
                          );

                          if (constraints.maxWidth < 1420) {
                            return ListView(
                              children: [
                                formPanel,
                                const SizedBox(height: 16),
                                systemPanel,
                                const SizedBox(height: 16),
                                SizedBox(height: 420, child: statusPanel),
                              ],
                            );
                          }

                          return Row(
                            crossAxisAlignment: CrossAxisAlignment.stretch,
                            children: [
                              Expanded(flex: 4, child: formPanel),
                              const SizedBox(width: 16),
                              Expanded(flex: 3, child: systemPanel),
                              const SizedBox(width: 16),
                              Expanded(flex: 4, child: statusPanel),
                            ],
                          );
                        },
                      ),
                    ),
                  ],
                );
              },
            ),
          ),
        ),
      ),
    );
  }
}
