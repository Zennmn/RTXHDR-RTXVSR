class SystemProbeModel {
  const SystemProbeModel({
    required this.osVersion,
    required this.gpuName,
    required this.driverVersion,
    required this.cudaVersion,
    required this.hasNvidiaGpu,
    required this.ngxVsrAvailable,
    required this.ngxTruehdrAvailable,
    required this.hdrExportAvailable,
    required this.h264HwDecode,
    required this.hevcHwDecode,
    required this.av1HwDecode,
    required this.h264HwEncode,
    required this.hevcHwEncode,
    required this.av1HwEncode,
    required this.supportedPixelFormats,
    required this.notes,
  });

  final String osVersion;
  final String gpuName;
  final String driverVersion;
  final String cudaVersion;
  final bool hasNvidiaGpu;
  final bool ngxVsrAvailable;
  final bool ngxTruehdrAvailable;
  final bool hdrExportAvailable;
  final bool h264HwDecode;
  final bool hevcHwDecode;
  final bool av1HwDecode;
  final bool h264HwEncode;
  final bool hevcHwEncode;
  final bool av1HwEncode;
  final String supportedPixelFormats;
  final String notes;
}
