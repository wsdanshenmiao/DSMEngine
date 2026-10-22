# TAA 稳定网格 GPU 回归

从仓库根目录执行：

```powershell
xmake build TemporalAATests
xmake run TemporalAATests
```

需要 Windows、可用 D3D12 GPU、Windows Graphics Tools 调试层和项目 DXC 运行库。目标输出到 `build/verification/temporal-aa/`；通过 `xmake run` 获取依赖运行环境。退出码 0 表示通过，1 表示回归失败，2 表示引擎启动失败。

测试运行真实 `TaaPass`、全部 shader 选项及 `MotionVectorPass`，通过 D3D12 原生纹理读回检查结果，不调用截图或替换渲染算法。测试不加载/保存用户场景。

## 用例与阈值

- 解析的二维线性渐变作为固定网格真值，生成随 Halton jitter 移动的颜色输入及带 jitter 差的速度；第三通道恒定以覆盖颜色裁剪中的零方向。
- 首帧、静止累积 40 帧、真实平移、历史坐标越界、非整数宽高比下 resize 首帧、16 种 shader 选项和单样本设置：排除四像素边界后，每通道最大绝对误差小于 `5e-5`，所有结果必须有限。越界用例关闭速度拒绝以隔离被测行为；另外确认 jitter 随帧变化，禁止通过关闭 jitter 绕过回归。
- 天空方向速度：相机平移不能影响天空速度，固定朝向时只保留帧间 jitter 差；另外检查旋转重投影，覆盖普通和 reversed-Z 深度。每通道误差不超过一个 FP16 ULP 加 `1e-6` UV 的计算误差，所有值必须有限；另报告未量化参考值对应的像素误差。
- 编译包含延迟渲染入口，检查共享接口不会依赖前向管线的偶然 include 顺序。
- GPU/RHI warning、error、fatal 使测试失败。

天空用例切换 reversed-Z 时同步重建深度资源的优化清除值，避免以不匹配的快速清除元数据触发调试层告警；不屏蔽任何调试层消息。

## 产物协议

任务验证记录写入 `build/verification/temporal-aa/status.raw.json`：顶层包含 gate、status、cwd、command、started_at、ended_at、exit_code、artifacts、diagnostics；gates 数组记录各构建/运行步骤及退出码。失败时先读取 diagnostics，再读取对应构建或 GPU 日志。独立执行测试时可以重定向 stdout/stderr 保存文本结果，测试程序自身不覆盖已有证据。

该测试覆盖坐标与重建契约，不代替真实场景中的运动遮挡、纹理高频细节和最终抗锯齿质量观察。

FLOAT32 到 FLOAT16 RenderTarget 转换采用向零取整，不能用固定的半个 ULP 或固定像素阈值比较不同量级的速度。规则见 Microsoft Direct3D 11.3 Functional Specification 的 3.2.2（D3D12 继承的基础格式转换规则）。

## 证据

2026-10-22，修复前 shader/CPU 实现使静止用例最大颜色误差约 `0.00185`，首帧、resize 和真实平移同样失败。修复后的结果及运行记录见 `Docs/ExecPlans/Completed/2026-10-22-StabilizeTemporalAAJitter.md`，临时日志不提交。
