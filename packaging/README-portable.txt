RTX Video Converter {{VERSION}}
================================

这是免安装便携版。请完整解压后运行 RTXVideoConverter.WinUI.exe，
不要只把主程序单独复制出来；runtime 目录和旁边的 DLL 都是必需组件。

运行要求：
- Windows 10 1809 或更高版本 / Windows 11
- 支持 RTX Video 的 NVIDIA RTX 显卡与较新驱动

输出视频默认保存在输入视频旁边。应用设置与历史记录保存在当前用户的
LocalAppData 目录，删除便携版目录不会自动删除这些用户数据。

安装、使用或再分发前请先阅读 DISTRIBUTION_TERMS.txt。项目自有代码、
FFmpeg 和 NVIDIA SDK 材料分别适用不同许可证，不能一概视为 MIT。

FFmpeg 的 LGPL 全文、第三方说明与 NVIDIA SDK 完整许可证见本目录内的
FFMPEG_LICENSE_LGPLv2.1.txt、THIRD_PARTY.md、
NV_CODEC_HEADERS_LICENSE.txt 和 NVIDIA_RTX_Video_SDK_License.pdf。

.NET、Windows App SDK、NuGet 运行库、cpp-httplib 和 nlohmann/json 的
精确版本清单见 THIRD_PARTY_NOTICES.txt；未经修改的原始许可与版权声明
位于 THIRD_PARTY_LICENSES 目录。

本版本所带 FFmpeg DLL 的完整对应源码、构建配置和可复现构建配方：
https://github.com/Zennmn/RTXHDR-RTXVSR/releases/download/v{{VERSION}}/RTX.Video.Converter_{{VERSION}}_FFmpeg-corresponding-source.zip
