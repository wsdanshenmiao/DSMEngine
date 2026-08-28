// DirectX Raytracing（DXR）端到端示例
// 演示 DSMEngine 的 RT 抽象层：构建 BLAS/TLAS、烘焙 ShaderTable、DispatchRays。
// 采用独立控制台程序，直接 CreateDevice，不依赖编辑器框架。

#include "TestTriangle.h"

int main()
{
    RayTracingTriangle();
    return 0;
}