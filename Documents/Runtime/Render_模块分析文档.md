# DSMEngine Runtime/Render 模块分析文档

## 文档概述

本文档详细分析 DSMEngine 的 **Runtime/Render** 模块，这是渲染系统的核心。该模块负责管理网格、材质、着色器、纹理和完整的前向渲染管道。支持现代渲染特性如阴影、SSAO、TAA 和各种后处理效果。

---

## 1. 代码结构与组织方式

### 1.1 目录结构

```
DSMEngine/Runtime/Render/
├── 资源管理
│   ├── Mesh.h/cpp                      # 网格数据
│   ├── Model.h/cpp                     # 模型容器
│   ├── Material.h/cpp                  # 材质
│   ├── Shader.h/cpp                    # 着色器
│   ├── ShaderCompiler.h/cpp            # 着色器编译器
│   ├── TextureManager.h/cpp            # 纹理管理
│   └── Geometry.h                      # 几何基础类
│
├── 渲染
│   ├── GraphicsRenderer.h/cpp          # 主渲染器
│   ├── Camera/
│   │   ├── Camera.h/cpp                # 相机
│   │   └── CameraController.h/cpp      # 相机控制
│   └── Renderer/
│       ├── RendererDX12.h              # D3D12 渲染器
│       └── ForwardRenderer/
│           ├── ForwardRenderPipeline.h # 前向渲染管道
│           ├── RenderResource.h/cpp    # 渲染资源
│           ├── GeometryPass.h          # G-Buffer 生成
│           ├── LitPass.h               # 光照通道
│           ├── ShadowPass.h/cpp        # 阴影通道
│           ├── SSAOPass.h              # AO 通道
│           ├── MotionVectorPass.h      # 运动矢量
│           ├── TaaPass.h               # TAA 通道
│           ├── SkyboxPass.h            # 天空盒
│           ├── PostEffect/
│           │   ├── BloomPass.h         # 泛光
│           │   └── ToneMappingPass.h   # 色调映射
│           └── FinalPass.h             # 最终通道
│
└── 工具
    └── WindowUI.h                      # UI 渲染
```

---

## 2. 核心功能与实现原理

### 2.1 模块职责

| 职责 | 描述 | 关键类 |
|------|------|--------|
| 网格管理 | 顶点/索引数据管理 | `Mesh` |
| 材质系统 | PBR 材质参数 | `Material` |
| 着色器编译 | HLSL 编译和缓存 | `ShaderCompiler` |
| 纹理管理 | 纹理加载和缓存 | `TextureManager` |
| 摄像机管理 | 视图和投影矩阵 | `Camera` |
| 前向渲染 | 多通道渲染管道 | `ForwardRenderPipeline` |
| 阴影系统 | 级联阴影贴图 | `ShadowPass` |
| 后处理 | Bloom、TAA 等 | `PostEffectManager` |

### 2.2 设计模式

#### 2.2.1 管道模式（Pipeline）

```cpp
// 各个渲染通道
class IRenderPass {
    virtual uint64_t Render(GraphicsRenderer& renderer, float deltaTime) = 0;
};

class ForwardRenderPipeline {
    std::vector<std::unique_ptr<IRenderPass>> m_RenderPasses;
    
    void Render() {
        for (auto& pass : m_RenderPasses) {
            pass->Render(renderer, deltaTime);
        }
    }
};
```

#### 2.2.2 资源工厂模式

```cpp
class TextureManager {
    std::unordered_map<std::string, TextureHandle> m_TextureCache;
    
    TextureHandle LoadTexture(const std::string& path) {
        if (auto it = m_TextureCache.find(path); it != m_TextureCache.end()) {
            return it->second;  // 缓存命中
        }
        auto texture = CreateTextureFromFile(path);
        m_TextureCache[path] = texture;
        return texture;
    }
};
```

---

## 3. 关键类、函数及其作用

### 3.1 Mesh 类（网格数据）

```cpp
struct Mesh {
    // 顶点属性
    enum VertexAttributeSlot { Position, UV, Normal, Tangent, Count };
    
    struct SubMesh {
        Math::AxisAlignedBox bounds;
        PrimitiveType primitiveType;
        size_t indexCount;
        size_t indexOffset;
        size_t vertexOffset;
    };
    
    // 网格数据
    std::string name;
    Format indexFormat;
    std::vector<uint8_t> indices;
    std::vector<Math::Vector3> vertices;
    std::vector<Math::Vector3> normals;
    std::vector<Math::Vector4> tangents;
    std::vector<Math::Vector2> uv;
    Math::AxisAlignedBox bounds;
    
    // 关键方法
    BufferHandle GetIndexBuffer() const;
    BufferHandle GetVertexBuffer() const;
    
    Mesh& SetIndices(std::span<const uint32_t> indices, 
                    PrimitiveType type, size_t subMeshIndex);
    Mesh& SetVertices(std::vector<Math::Vector3> vertices);
    Mesh& SetNormals(std::vector<Math::Vector3> normals);
    Mesh& SetUVs(std::vector<Math::Vector2> uv);
    
    const SubMesh& GetSubMesh(size_t index) const;
    void UploadBuffer();
    
    // 静态管理
    static void Create(IDevice* device);
    static void Destroy();
};
```

**关键特性：**
- 支持多个 SubMesh
- 自动计算边界框
- 模板化索引格式支持
- GPU 缓冲自动上传

### 3.2 Material 类（PBR 材质）

```cpp
class Material {
public:
    Material(std::shared_ptr<Shader> shader);
    
    // 颜色属性
    const Math::Vector4& GetBaseColor() const;
    void SetBaseColor(const Math::Vector4& color);
    
    const Math::Vector4& GetEmissiveColor() const;
    void SetEmissiveColor(const Math::Vector4& color);
    
    // PBR 参数
    float GetMetallicFactor() const;
    void SetMetallicFactor(float factor);
    
    float GetRoughnessFactor() const;
    void SetRoughnessFactor(float factor);
    
    float GetNormalTexScale() const;
    void SetNormalTexScale(float scale);
    
    // 纹理管理
    TextureHandle GetTexture(ShaderResource::MaterialTex index) const;
    void SetTexture(ShaderResource::MaterialTex index, 
                   const TextureHandle& texture);
    
    // 渲染选项
    bool IsBothSide() const;
    void SetBothSide(bool bothSide);
    
    bool IsTransparent() const;
    void SetTransparent(bool transparent);
    
    // 着色器控制
    void EnableKeyword(const std::string& keyword, 
                      const std::string& value);
    void FindPass(const std::string& entryPoint, ShaderType type);
    
private:
    std::shared_ptr<Shader> m_Shader;
    std::array<TextureHandle, ShaderResource::kNumTextures> m_Textures;
    Math::Vector4 m_BaseColor{1, 1, 1, 1};
    Math::Vector4 m_EmissiveColor{0, 0, 0, 0};
    float m_MetallicFactor{0.0f};
    float m_RoughnessFactor{1.0f};
    float m_NormalTexScale{1.0f};
    bool m_BothSide{false};
    bool m_Transparent{false};
};
```

### 3.3 Camera 类（摄像机）

```cpp
class Camera {
public:
    // 视图矩阵
    const Matrix4& GetViewMatrix() const;
    const Matrix4& GetProjectionMatrix() const;
    const Matrix4& GetViewProjectionMatrix() const;
    
    // 参数设置
    void SetPosition(const Vector3& pos);
    void SetTarget(const Vector3& target);
    void SetFOV(float fov);
    void SetAspectRatio(float aspect);
    void SetNearPlane(float near);
    void SetFarPlane(float far);
    
    // 获取参数
    Vector3 GetPosition() const;
    Vector3 GetTarget() const;
    float GetFOV() const;
    
    // 射线投射
    Ray ScreenPointToRay(const Vector2& screenPos);
    Vector3 WorldToScreenPoint(const Vector3& worldPos);
};
```

### 3.4 GraphicsRenderer 类（主渲染器）

```cpp
class GraphicsRenderer {
public:
    // 初始化
    void Initialize(const DeviceDesc& deviceDesc);
    void Shutdown();
    
    // 渲染管道
    void SetRenderPipeline(std::unique_ptr<IRenderPipeline> pipeline);
    
    // 渲染
    void Render(float deltaTime);
    
    // 资源访问
    IDevice* GetDevice() const;
    Camera& GetCamera();
    Scene* GetScene();
    
    // 纹理和材质
    TextureHandle LoadTexture(const std::string& path);
    std::shared_ptr<Mesh> LoadMesh(const std::string& path);
    std::shared_ptr<Material> CreateMaterial(std::shared_ptr<Shader> shader);
    
    // 交换链管理
    TextureHandle GetCurrentBackBuffer() const;
    void Resize(uint32_t width, uint32_t height);
    
private:
    std::shared_ptr<IDevice> m_Device;
    std::unique_ptr<IRenderPipeline> m_RenderPipeline;
    Camera m_Camera;
    TextureManager m_TextureManager;
    // ... 其他成员
};
```

### 3.5 渲染通道（RenderPass）

所有渲染通道实现此接口：

```cpp
class IRenderPass {
public:
    virtual ~IRenderPass() = default;
    virtual uint64_t Render(GraphicsRenderer& renderer, 
                          float deltaTime) = 0;
    virtual void OnResize(GraphicsRenderer& renderer, 
                         uint32_t width, uint32_t height) = 0;
};

// 具体通道示例
class GeometryPass : public IRenderPass {
    uint64_t Render(GraphicsRenderer& renderer, float deltaTime) override;
    void OnResize(GraphicsRenderer& renderer, 
                 uint32_t width, uint32_t height) override;
};

class LitPass : public IRenderPass {
    uint64_t Render(GraphicsRenderer& renderer, float deltaTime) override;
    void OnResize(GraphicsRenderer& renderer, 
                 uint32_t width, uint32_t height) override;
};

class ShadowPass : public IRenderPass {
    uint64_t Render(GraphicsRenderer& renderer, float deltaTime) override;
    void OnResize(GraphicsRenderer& renderer, 
                 uint32_t width, uint32_t height) override;
};

class PostEffectManager : public IRenderPass {
    void AddEffect(std::unique_ptr<IPostEffect> effect);
    uint64_t Render(GraphicsRenderer& renderer, float deltaTime) override;
};
```

---

## 4. 渲染管道流程

### 4.1 单帧渲染流程

```
Update(deltaTime)
    ↓
┌─────────────────────────────────┐
│ 1. GeometryPass                 │ - 生成 G-Buffer
│    输出：位置、法线、反照率等   │
└─────────────────────────────────┘
    ↓
┌─────────────────────────────────┐
│ 2. MotionVectorPass             │ - 计算运动矢量
│    输出：运动矢量纹理           │
└─────────────────────────────────┘
    ↓
┌─────────────────────────────────┐
│ 3. SSAOPass                     │ - 屏幕空间环境光遮蔽
│    输出：AO 纹理                │
└─────────────────────────────────┘
    ↓
┌─────────────────────────────────┐
│ 4. ShadowPass                   │ - 生成级联阴影贴图
│    输出：阴影贴图               │
└─────────────────────────────────┘
    ↓
┌─────────────────────────────────┐
│ 5. LightingPass / LitPass       │ - 光照计算
│    输出：光照结果               │
└─────────────────────────────────┘
    ↓
┌─────────────────────────────────┐
│ 6. SkyboxPass                   │ - 绘制天空盒
│    输出：天空贡献               │
└─────────────────────────────────┘
    ↓
┌─────────────────────────────────┐
│ 7. TaaPass                      │ - 时间抗锯齿
│    输出：抗锯齿结果             │
└─────────────────────────────────┘
    ↓
┌─────────────────────────────────┐
│ 8. PostEffectManager            │ - 后处理
│    ├─ Bloom                     │
│    ├─ Blur                      │
│    └─ ToneMapping               │
│    输出：最终图像               │
└─────────────────────────────────┘
    ↓
┌─────────────────────────────────┐
│ 9. FinalPass                    │ - 最终处理
│    输出：交换链                 │
└─────────────────────────────────┘
```

### 4.2 通道间数据传递

```
G-Buffer 输出：
├─ GBuffer0：位置 (RGB32F)
├─ GBuffer1：法线 (RGB32F)
├─ GBuffer2：反照率 (RGBA8U)
├─ GBuffer3：金属粗糙度 (RG32F)
└─ 深度缓冲：D32F

ShadowPass 输出：
├─ 级联阴影0：2K×2K
├─ 级联阴影1：2K×2K
├─ 级联阴影2：2K×2K
└─ 级联阴影3：2K×2K

LitPass 使用：
├─ G-Buffer 所有输出
├─ 阴影贴图
├─ SSAO
├─ 光源数据
└─ 输出：HDR 图像
```

---

## 5. 使用示例

### 5.1 加载和渲染网格

```cpp
// 创建网格
auto mesh = std::make_shared<Mesh>();
mesh->SetName("TestMesh");
mesh->SetIndexFormat(Format::R32_UINT);

// 设置顶点数据
std::vector<Vector3> vertices = {
    {-1, -1, 0}, {1, -1, 0}, {1, 1, 0}, {-1, 1, 0}
};
std::vector<uint32_t> indices = {0, 1, 2, 0, 2, 3};

mesh->SetVertices(vertices);
mesh->SetIndices(indices, PrimitiveType::TriangleList, 0);
mesh->UploadBuffer();

// 在游戏对象上使用
auto renderer = gameObject->AddComponent<MeshRenderer>();
renderer->SetMesh(mesh);
```

### 5.2 创建 PBR 材质

```cpp
// 加载着色器
auto shader = ShaderCompiler::Compile("Assets/Shaders/PBR.hlsl");
auto material = std::make_shared<Material>(shader);

// 设置 PBR 参数
material->SetBaseColor({1.0f, 0.5f, 0.2f, 1.0f});
material->SetMetallicFactor(0.8f);
material->SetRoughnessFactor(0.3f);

// 加载纹理
auto albedo = renderer->LoadTexture("Assets/Textures/rust_albedo.png");
auto normal = renderer->LoadTexture("Assets/Textures/rust_normal.png");

material->SetTexture(ShaderResource::MaterialTex::Albedo, albedo);
material->SetTexture(ShaderResource::MaterialTex::Normal, normal);

// 应用材质
renderer->SetMaterial(material);
```

### 5.3 摄像机控制

```cpp
// 初始化摄像机
auto& camera = graphicsRenderer.GetCamera();
camera.SetPosition({0, 2, 5});
camera.SetTarget({0, 0, 0});
camera.SetFOV(60.0f);
camera.SetNearPlane(0.1f);
camera.SetFarPlane(1000.0f);

// 获取矩阵用于渲染
Matrix4 viewProj = camera.GetViewProjectionMatrix();
```

---

## 6. 性能优化

### 6.1 批处理优化

```cpp
// ✅ 好：批处理相同材质的对象
for (auto& batch : materialBatches) {
    pipeline->BindMaterial(batch.material);
    for (auto& mesh : batch.meshes) {
        pipeline->DrawMesh(mesh);
    }
}

// ❌ 不好：每个对象切换材质
for (auto& obj : objects) {
    pipeline->BindMaterial(obj.material);
    pipeline->DrawMesh(obj.mesh);
}
```

### 6.2 纹理缓存

```cpp
// TextureManager 自动缓存
auto tex1 = renderer->LoadTexture("Assets/Texture.png");
auto tex2 = renderer->LoadTexture("Assets/Texture.png");
// tex1 == tex2（缓存命中，无重复加载）
```

### 6.3 阴影优化

```cpp
// 级联阴影配置
ShadowPass::sm_Setting.cascadeCount = 4;
ShadowPass::sm_Setting.cascadeFarPlaneDist = {10, 50, 200, 1000};
ShadowPass::sm_Setting.directionalSetting.filter = 
    ShadowSetting::FilterMode::PCF5;  // 5x5 PCF
```

---

## 7. 常见问题

### Q1: 如何添加自定义后处理效果？

**A:**

```cpp
class CustomEffect : public IPostEffect {
    uint64_t Render(GraphicsRenderer& renderer, 
                   ICommandList* cmdList) override {
        // 实现自定义效果
    }
};

// 添加到管道
auto& postMgr = dynamic_cast<PostEffectManager&>(*renderPipeline);
postMgr.AddEffect(std::make_unique<CustomEffect>());
```

### Q2: 如何改进阴影质量？

**A:**

```cpp
// 增加 PCF 采样
ShadowPass::sm_Setting.directionalSetting.filter = 
    ShadowSetting::FilterMode::PCF7;

// 增加级联数量
ShadowPass::sm_Setting.cascadeCount = 4;

// 调整级联范围
ShadowPass::sm_Setting.cascadeFarPlaneDist = 
    {10, 50, 200, 1000};
```

---

## 术语表

| 术语 | 定义 |
|------|------|
| **G-Buffer** | 几何缓冲，存储表面属性 |
| **PBR** | 基于物理的渲染 |
| **TAA** | 时间抗锯齿 |
| **SSAO** | 屏幕空间环境光遮蔽 |
| **PCF** | 百分比接近滤波（阴影) |
| **HDR** | 高动态范围 |
| **Bloom** | 泛光效果 |

---

**文档版本：** v1.0  
**最后更新：** 2026-04-16
