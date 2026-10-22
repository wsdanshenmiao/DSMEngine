# 修复 TAA 亚像素抖动

状态：completed

## 摘要 (Summary)

保留 Halton jitter；静止相机下，TAA 输出与历史颜色使用固定像素网格，不再随采样序列移动。保持前向/延迟管线接口及默认 TAA 参数；不修改场景、vendor、Present 或相机输入。

## 背景 (Context)

- 用户确认 GetJitterOffset 返回零后抖动消失。实施前该临时改动已撤回；唯一已有修改为 Projects/PBR/Content/Scenes/Untitled.dsmscene，必须保留。SHA256 为 CF87997855D224567E91859B089A09C1F1D3B0A61AC7B2319F509F964D8C6DA1。
- Geometry/Lit/GBuffer 与 MotionVectorPass 使用抖动投影；速度为 vJ = currUVJ - prevUVJ。旧 resolve 将历史对齐到当前抖动网格，并把 jitter 差当成真实运动降低历史权重。
- 外部契约：当前/上一帧图像 UV 位移为 Jc/Jp，输出坐标为 u；当前帧在 r = u + Jc 重建，历史坐标为 r - vJ(r) - Jp，等价于 u - (vJ(r) - (Jc-Jp))。拒绝只使用真实速度 vJ-(Jc-Jp)。NDC 到 UV 的 Y 轴需要取反。
- SSAO、tile lighting、skybox 与抖动深度/几何坐标必须一致。天空速度忽略相机平移，但保留旋转和 jitter。
- 当前为 Git 仓库，根目录和父目录无 p4config.txt，不使用默认 Perforce 工作区。

## 实施计划 (Implementation Plan)

1. 在 CommonPass/TaaPass.h 统一投影 jitter 与 UV 位移；传递当前 jitter 与帧间差。首帧及 resize 后也走稳定网格 resolve，禁止直接拷贝未稳定图像作为历史。
2. 修改 Engine/Shaders/ForwardShader/Passes/TaaPass.hlsl：对齐当前颜色、邻域、深度/速度查询，固定历史网格；拒绝越界历史；避免颜色裁剪零方向产生 NaN。
3. Geometry/Lit/GBuffer/DeferredLighting/SSAO/Lighting/Skybox 使用统一投影函数；保留带 jitter 的速度契约，天空按方向重投影。
4. 增加真实 TAA shader GPU 回归：非零 jitter 静态渐变、首帧、resize、真实平移、越界与 shader 选项。只测试渲染行为，不测试类型系统保证的内容。
5. 构建 PBR 与测试目标，执行 GPU 回归；尝试启动当前 PBR 场景，区分自动证据与人工观察，完成后归档。

## 验证 (Validation)

默认工作目录为 D:\Code\DSMEngine；产物放 build/verification/temporal-aa/。

- xmake build PBR：覆盖前向管线和 Editor。
- 新增 TemporalAATests 目标并执行：真实 D3D12、TaaPass/shader、纹理读回；记录误差与非有限值。
- 回归测试在修复前实现上应失败、修复后通过，保留前后日志。
- 延迟管线至少编译包含入口；TaaPass 构造时编译全部 shader 选项。
- 使用可用入口启动 Projects/PBR/PBR.dsmproj 的当前 Untitled 场景；不修改场景，不以进程存活冒充视觉验证。
- git diff --check；确认场景文件哈希不变。

## 进展 (Progress)

- [x] 阅读规则和验证入口，确认已有改动。
- [x] 明确稳定输出网格、带 jitter 速度和 UV 轴向契约。
- [x] 实施固定网格 resolve、统一投影与天空方向速度修复。
- [x] 修复前 GPU 回归失败；修复后全部颜色/历史用例与天空平移、reversed-Z 用例通过。
- [x] xmake PBR、CMake engine/pbr configure 与 CMake TemporalAATests 构建/运行通过。
- [x] 实际 PBR Untitled 场景启动并持续运行，退出前确认场景哈希不变。
- [x] 补充天空旋转回归；使用 FP16 ULP 容差核实实际 RG16_FLOAT 输出。
- [x] 定位并修正测试资源的 reversed-Z 快速清除元数据；D3D12 调试层没有 warning/error，未屏蔽消息。
- [x] 最终 xmake 全量构建、CMake 测试构建、单轮 GPU 回归全部通过；汇总到 status.raw.json。
- [x] 记录验证边界、保留场景哈希、完成差异复核与归档。

## 意外与发现 (Surprises & Discoveries)

- 源码已恢复 Halton jitter，不能把先前的 return 0 当作基线。
- 旧代码初次积累直接 CopyTexture，需要与普通帧一起修复。
- 首次天空回归失败是测试自身继承了上一组 reversed-Z 相机状态，导致使用错误 clearDepth；明确重置相机状态后通过，未以修改实现掩盖测试环境问题。
- PBR 项目描述 name 为空，Tools/dsm.ps1 的严格入口不适用；使用 xmake run PBR --project 的现有支持入口，未修改用户项目描述。
- 额外启用调试层检查后，测试曾因两条 D3D12 821 警告失败：测试把声明优化清除值为 1 的深度资源清成 0。切换 reversed-Z 时同步重建测试深度资源及其 framebuffer，最终检查通过；生产 TAA 颜色用例没有因此改阈值或屏蔽调试层。
- 天空旋转时速度较大，固定 0.003 像素阈值不足以覆盖 RG16_FLOAT 向零取整。依据 Direct3D 格式转换规则按一个 FP16 ULP 加 1e-6 UV 计算误差验收；实际最大参考像素差 0.005650520 在该量化界限内，不是位置重建漂移。

## 决策记录 (Decision Log)

- 保留带 jitter 的速度和深度逆矩阵，避免用未抖动逆矩阵重建抖动深度。
- 显式重建稳定当前颜色及历史坐标，不靠关闭 jitter 或提高历史权重掩盖问题。
- 不扩展到视口浮点尺寸、完整 HDR 管线和透明速度策略。
- 历史越界用例关闭速度拒绝，防止大速度把历史权重清零后掩盖被测行为；测试明确要求 jitter 随帧变化，不能关闭 jitter 使回归通过。
- 以实际 shader 纹理读回、前后失败/成功对比和调试层作为本轮主要验收证据；没有人工视觉访问，不宣称完成了真实场景逐帧画面对比。

## 结果与复盘 (Outcomes & Retrospective)

已获得的证据：

- build/verification/temporal-aa/baseline.log：旧实现首帧误差 0.001000017、静止/真实平移 0.001851916、resize 0.000666678，均失败。
- build/verification/temporal-aa/final.log：最终修复后首帧 0.000005275、静止 0.000004083、真实平移 0.000004709、历史越界 0.000002980、resize 0.000005245、全部 shader 选项 0.000007033，均低于 0.00005；单样本、天空平移/旋转、普通/reversed-Z 与 D3D12 调试层检查均通过。
- build/verification/temporal-aa/build-pbr.log：xmake build PBR 通过。
- build/verification/temporal-aa/cmake-engine.log、cmake-pbr.log：CMake 4.3.1-msvc1 配置通过。最终 cmake-build-tests-final.log 构建通过，xmake build-all-final.log 全量构建通过；为避免重复弹窗，最终相同源码 GPU 回归只通过 Xmake 入口执行一次。
- build/verification/temporal-aa/status.raw.json：最终三个 gate 均为 pass，退出码均为 0，含命令、时间和日志。debug-layer-before-fixture-fix.log 保留此前调试层告警，不用失败日志冒充最终成功证据。
- pbr-smoke.observed.json/stdout/stderr.log：RTX 3080，实际 PBR 从 20:25:35 运行至 20:27:57，窗口保持响应且标题持续报告 FPS，stderr 为空。仅做启动/帧循环检查，没有截图或人工视觉对比，不把进程存活当成画面修复证明。
- 为避免 Editor 正常退出自动保存用户场景，仅终止本次启动并核实父子关系的 PBR PID 50672；原场景 SHA256 与实施前一致。

剩余边界：GPU 渐变回归证明坐标稳定，不代替真实场景中高频纹理、运动遮挡和抗锯齿质量的人工观察；未扩展到透明物体速度、历史深度遮挡拒绝或高阶重建滤波。

最终交付：统一的抖动投影函数、固定输出网格的 TAA resolve（包含首帧/resize）、真实速度驱动的历史权重及越界拒绝、与深度一致的 SSAO/tile lighting/skybox、正确的天空方向速度；新增 Tests/TemporalAA/ 下真实 GPU 回归及 Xmake/CMake 目标。未修改默认 TAA 参数，未关闭 jitter，未修改用户场景，也未提交 Git commit。
