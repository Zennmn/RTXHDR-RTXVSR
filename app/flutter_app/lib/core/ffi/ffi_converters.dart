import 'dart:convert';
import 'dart:ffi';

import 'package:ffi/ffi.dart';

String readFixedString(Array<Uint8> buffer, int length) {
  final bytes = <int>[];
  for (var index = 0; index < length; index += 1) {
    final value = buffer[index];
    if (value == 0) {
      break;
    }
    bytes.add(value);
  }
  return utf8.decode(bytes, allowMalformed: true);
}

Pointer<Utf8> toNativeUtf8OrNull(String value) {
  if (value.isEmpty) {
    return nullptr;
  }
  return value.toNativeUtf8();
}
