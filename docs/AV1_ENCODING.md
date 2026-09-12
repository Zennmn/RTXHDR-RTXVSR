# AV1 编码修复与验证

本次调整针对 NVENC AV1 体积偏大，以及部分软件解码器拒绝自动生成的 AV1 等级的问题。

## 最终行为

- AV1 使用 VBR / CQ 24；HEVC、H.264 保持 CQ 18。不同编码格式的 CQ 数字不是等画质刻度，CQ 24 是偏向体积的有损折中，不承诺每个片源都比 HEVC 更小或画质完全相同。
- 保留 P7、HQ、AQ、32 帧 lookahead、全分辨率 multipass 和已有分帧策略。短片对比未证明关闭分帧能改善本次问题，因此不额外改变它。
- AV1 根据输出尺寸、帧率、最大码率和 VBV 缓冲区选择已定义的 level / tier，不再依赖自动 level。NVENC 继续负责选定等级的其他编码约束。
- 常见中等码率 4K60 选择 5.1；高码率或较大缓冲区可能要求更高等级，8K 也不写死为 5.1。超出当前支持的等级容量会明确报错，不回退到未定义的等级。
- 对 HDR、SDR/VSR 两种 AV1 输出都应用该策略；HDR 色彩标记和其他处理参数保持原样。旧文件不会被自动修改，需要重新转码。
- 日志增加实际 CQ、AV1 level index 和 tier，便于复核。

## 验证环境与结果

环境：RTX 5070 Ti，驱动 616.92；原便携包 FFmpeg 动态库（源码版本 `a09be9b91e8e1219f297586873b0d7322b47df96`），RTX Video SDK 1.1.0。

1. 软件解码问题可复现：自动 level 输出 `seq_level_idx=23`，libaom 报该值尚未定义。相同短片显式 5.1 后体积相同，完整软件解码成功。这不是直接篡改成品文件头。
2. 以用户提供的 HEVC 成品为共同参考，在第 5、35、65 秒各取 2 秒，使用相同 NVENC 设置重新编码。结果如下（单位为十进制 MB）：

| 片段起点 | HEVC CQ18 | AV1 CQ18 | AV1 CQ24 |
|---|---:|---:|---:|
| 5 秒 | 4.875 | 8.675 | 4.150 |
| 35 秒 | 6.215 | 10.161 | 5.135 |
| 65 秒 | 5.877 | 9.797 | 4.696 |

这是对已压缩 HDR 成品的重编码试验，不是对原始素材的等画质认证。CQ24 相对该参考的 YUV10 SSIM 约 0.9978；该指标不等同于 HDR 感知质量。

3. 使用最终便携目录的真实后端对同一合成 SDR 4K60 视频进行完整 RTX HDR 转码：HEVC 新旧输出逐字节相同；AV1 从 22,369,816 字节降为 16,116,549 字节，120 帧完整软件解码成功。该高码率测试的缓冲区要求选择 6.1。另一中等码率 4K60 测试正确选择 5.1，120 帧完整软件解码成功。
4. AV1 的纯 HDR、纯 VSR/SDR、VSR+HDR 路径均通过 GPU 转码及软件解码检查。
5. 83 项 C++ 测试通过，包括常见分辨率、59.94fps、4K120、8K、码率/缓冲区边界、无效输入和超范围输出的等级选择测试。

## 依据

- [NVIDIA NVENC 码率控制说明](https://docs.nvidia.com/video-technologies/video-codec-sdk/13.0/nvenc-video-encoder-api-prog-guide/index.html#rate-control)
- [AOM AV1 Annex A 等级限制](https://github.com/AOMediaCodec/av1-spec/blob/master/annex.a.levels.md)

本项目的 level 数值是 AV1 的 `seq_level_idx`，例如 13 对应 5.1，17 对应 6.1；不能将 13 解释成“level 13”。
