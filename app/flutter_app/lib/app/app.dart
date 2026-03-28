import 'dart:async';

import 'package:flutter/material.dart';
import 'package:flutter_localizations/flutter_localizations.dart';

import '../features/job/application/export_job_controller.dart';
import '../features/job/presentation/export_dashboard_page.dart';
import '../features/system/application/system_probe_controller.dart';
import 'theme/app_theme.dart';

class RtxHdrRtxVsrApp extends StatefulWidget {
  const RtxHdrRtxVsrApp({super.key});

  @override
  State<RtxHdrRtxVsrApp> createState() => _RtxHdrRtxVsrAppState();
}

class _RtxHdrRtxVsrAppState extends State<RtxHdrRtxVsrApp> {
  late final SystemProbeController _systemProbeController;
  late final ExportJobController _exportJobController;

  @override
  void initState() {
    super.initState();
    _systemProbeController = SystemProbeController();
    _exportJobController = ExportJobController();
    unawaited(_exportJobController.restoreLastDraft());
    WidgetsBinding.instance.addPostFrameCallback((_) {
      _systemProbeController.load();
    });
  }

  @override
  void dispose() {
    _systemProbeController.dispose();
    _exportJobController.dispose();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    return MaterialApp(
      title: 'RTXHDR+RTXVSR 视频增强工具',
      debugShowCheckedModeBanner: false,
      theme: AppTheme.light(),
      darkTheme: AppTheme.dark(),
      locale: const Locale('zh', 'CN'),
      supportedLocales: const [Locale('zh', 'CN'), Locale('en', 'US')],
      localizationsDelegates: const [
        GlobalMaterialLocalizations.delegate,
        GlobalWidgetsLocalizations.delegate,
        GlobalCupertinoLocalizations.delegate,
      ],
      home: ExportDashboardPage(
        systemProbeController: _systemProbeController,
        exportJobController: _exportJobController,
      ),
    );
  }
}
