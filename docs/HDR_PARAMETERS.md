# HDR 参数刻度

| 参数 | 界面可选范围 | 界面默认值 | 提交给后端 / RTX Video SDK |
|---|---|---|---|
| 对比度 | 0～200 | 100 | 原值 |
| 饱和度 | 0～200 | 100 | 原值 |
| 中灰度 | 10～100 | 44 | 原值，不加减 100 |
| 最大亮度 | 400～2000 nit | 1000 nit | 原值，单位 nit |

界面提供离散档位。对比度、饱和度每档 25；中灰度保留原档位并覆盖 10～100；最大亮度为 400、600、1000、1500、2000 nit。

对比度、饱和度采用后端原始刻度，以 100 为默认值。该刻度不表示输出像素或感知效果按百分比线性变化。对比度 0 也不表示关闭 HDR。

旧界面仅提供对比度 / 饱和度 50～150，新界面补齐到 0～200。所有选项均原值提交，不加减 100。默认提交参数仍为 100、100、44、1000，默认处理效果保持不变。后端 JSON 协议未改变。

NVIDIA App 的部分 RTX HDR 控件采用 −100～100 的居中显示，本项目按后端 0～200 显示。NVIDIA App 的内部映射未公开核实，不保证游戏 RTX HDR 与 RTX Video SDK 的输出完全相同。

范围参考：

- [NVIDIA TrueHDR 参数文档](https://docs.nvidia.com/maxine/vfx/1.3.0/Filters/TrueHDR.html)（VFX SDK 文档，用于交叉核对；其默认中灰度和峰值亮度不同于本项目）
- [TouchDesigner 的 RTX Video SDK 集成](https://github.com/TouchDesigner/NVIDIARTXVideoTOP/blob/main/src/NVIDIARTXVideoTOP.cpp)（对比度、饱和度 0～2 乘以 100 后传给 SDK，与本项目后端范围一致）
