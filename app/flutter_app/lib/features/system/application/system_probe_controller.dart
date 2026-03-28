import 'dart:isolate';

import 'package:flutter/foundation.dart';

import '../../../core/ffi/native_library.dart';
import '../domain/system_probe_model.dart';

class SystemProbeController extends ChangeNotifier {
  SystemProbeModel? probe;
  String? errorMessage;
  bool isLoading = false;

  Future<void> load() async {
    isLoading = true;
    errorMessage = null;
    notifyListeners();

    try {
      await Future<void>.delayed(const Duration(milliseconds: 600));
      probe = await _probeSystemInBackground();
    } catch (error) {
      errorMessage = error.toString();
    } finally {
      isLoading = false;
      notifyListeners();
    }
  }

  Future<SystemProbeModel> _probeSystemInBackground() async {
    final data = await Isolate.run(() {
      final probe = NativeLibrary.instance.probeSystem();
      return <String, Object?>{
        'osVersion': probe.osVersion,
        'gpuName': probe.gpuName,
        'driverVersion': probe.driverVersion,
        'cudaVersion': probe.cudaVersion,
        'hasNvidiaGpu': probe.hasNvidiaGpu,
        'ngxVsrAvailable': probe.ngxVsrAvailable,
        'ngxTruehdrAvailable': probe.ngxTruehdrAvailable,
        'hdrExportAvailable': probe.hdrExportAvailable,
        'h264HwDecode': probe.h264HwDecode,
        'hevcHwDecode': probe.hevcHwDecode,
        'av1HwDecode': probe.av1HwDecode,
        'h264HwEncode': probe.h264HwEncode,
        'hevcHwEncode': probe.hevcHwEncode,
        'av1HwEncode': probe.av1HwEncode,
        'supportedPixelFormats': probe.supportedPixelFormats,
        'notes': probe.notes,
      };
    });

    return SystemProbeModel(
      osVersion: data['osVersion']! as String,
      gpuName: data['gpuName']! as String,
      driverVersion: data['driverVersion']! as String,
      cudaVersion: data['cudaVersion']! as String,
      hasNvidiaGpu: data['hasNvidiaGpu']! as bool,
      ngxVsrAvailable: data['ngxVsrAvailable']! as bool,
      ngxTruehdrAvailable: data['ngxTruehdrAvailable']! as bool,
      hdrExportAvailable: data['hdrExportAvailable']! as bool,
      h264HwDecode: data['h264HwDecode']! as bool,
      hevcHwDecode: data['hevcHwDecode']! as bool,
      av1HwDecode: data['av1HwDecode']! as bool,
      h264HwEncode: data['h264HwEncode']! as bool,
      hevcHwEncode: data['hevcHwEncode']! as bool,
      av1HwEncode: data['av1HwEncode']! as bool,
      supportedPixelFormats: data['supportedPixelFormats']! as String,
      notes: data['notes']! as String,
    );
  }
}
