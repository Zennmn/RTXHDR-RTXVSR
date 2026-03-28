# Flutter Windows 客户端

这是 `RTXHDR+RTXVSR` 的 Flutter Windows 桌面端。

主要职责：

- 提供导出参数配置界面
- 展示系统能力探测结果
- 通过 FFI 调用原生 `rtx_native.dll`
- 轮询任务进度、日志与错误信息
- 自动保存并恢复上次使用的导出选项

如果你想了解完整项目说明、依赖准备和构建方式，请优先阅读仓库根目录下的 `README.md`。
