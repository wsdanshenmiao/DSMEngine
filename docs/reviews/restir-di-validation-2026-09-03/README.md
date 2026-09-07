# ReSTIR DI 渲染验证证据（2026-09-03）

## 范围

这组产物来自 `RestirDI.exe --validate-render` 的 320×180 固定验证场景，原先位于 `.tmp/verification/raytracing-entry-fix-20260903`。为避免将可追溯证据与一次性临时文件混在一起，现归档到 `docs/reviews`。

## 结论

- `status.raw.json`：`passed = true`，退出码 0。
- `metrics.json`：HDR 有限比例 1.0；1/2/4/8 SPP 均通过质量检查。
- `d3d12-dxgi-messages.json`：D3D12 和 DXGI 消息列表均为空。
- `runtime-messages.json`：只包含 RTX 3080 设备选择信息，无 Warning/Error/Fatal。
- Alpha 透明/不透明探针、`CastShadow=false`、Resize、HDR 加载与三种候选域均通过。

这是一轮历史验证快照，不替代当前源码的再次运行。当前实现的最新验证入口和指标说明见 [`../../knowledge/restir-di-sample-implementation.md`](../../knowledge/restir-di-sample-implementation.md)。

## 图像索引

- `restir.bmp` / `reference.bmp`：完整 ReSTIR 与高样本参考。
- `spp-1.bmp`、`spp-2.bmp`、`spp-4.bmp`、`spp-8.bmp`：SPP 质量阶梯。
- `analytic.bmp`、`emissive.bmp`、`environment.bmp`：独立候选域。
- `motion.bmp`：运动历史验证。
- `alpha.bmp`：Alpha any-hit 验证。
- `no-shadow.bmp`：Primary 可见但不投影验证。
- `source-debug.bmp`：候选源调试视图。
