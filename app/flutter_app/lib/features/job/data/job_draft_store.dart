import 'dart:convert';
import 'dart:developer' as developer;
import 'dart:io';

import 'package:path/path.dart' as p;

import '../domain/job_models.dart';

/// Persists the last used export options for the desktop app.
class JobDraftStore {
  const JobDraftStore();

  Future<JobConfigDraft?> load() async {
    try {
      final file = await _resolveFile();
      if (!await file.exists()) {
        return null;
      }

      final raw = await file.readAsString();
      final decoded = jsonDecode(raw);
      if (decoded is! Map<String, dynamic>) {
        return null;
      }

      final draftData = decoded['draft'];
      if (draftData is! Map<String, dynamic>) {
        return null;
      }

      return JobConfigDraft.fromJson(draftData);
    } catch (error, stackTrace) {
      developer.log(
        'Failed to load persisted export draft.',
        name: 'rtxhdr_rtxvsr.job_draft_store',
        error: error,
        stackTrace: stackTrace,
      );
      return null;
    }
  }

  Future<void> save(JobConfigDraft draft) async {
    try {
      final file = await _resolveFile();
      await file.parent.create(recursive: true);
      final payload = <String, Object?>{
        'schemaVersion': 1,
        'savedAt': DateTime.now().toIso8601String(),
        'draft': draft.toJson(),
      };
      final formatted = const JsonEncoder.withIndent('  ').convert(payload);
      await file.writeAsString(formatted);
    } catch (error, stackTrace) {
      developer.log(
        'Failed to persist export draft.',
        name: 'rtxhdr_rtxvsr.job_draft_store',
        error: error,
        stackTrace: stackTrace,
      );
    }
  }

  Future<File> _resolveFile() async {
    final root =
        Platform.environment['LOCALAPPDATA'] ??
        Platform.environment['APPDATA'] ??
        Platform.environment['USERPROFILE'] ??
        Directory.current.path;
    return File(p.join(root, 'RTXHDR_RTXVSR', 'last_export_options.json'));
  }
}
