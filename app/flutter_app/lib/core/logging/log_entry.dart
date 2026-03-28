class LogEntry {
  const LogEntry({
    required this.level,
    required this.phase,
    required this.message,
    required this.timestampMs,
  });

  final String level;
  final String phase;
  final String message;
  final int timestampMs;
}

extension LogEntryLocalization on LogEntry {
  String get localizedLevel {
    switch (level) {
      case 'error':
        return '错误';
      case 'warn':
        return '警告';
      case 'info':
        return '信息';
      case 'debug':
        return '调试';
      default:
        return level;
    }
  }
}
