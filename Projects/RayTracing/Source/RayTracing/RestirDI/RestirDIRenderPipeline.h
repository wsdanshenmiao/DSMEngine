#pragma once

#include "RestirDIShared.h"
#include "RestirDISettings.h"
#include "Runtime/Render/Renderer/GraphicsRenderer.h"

#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace DSM::RestirDI {

    struct ValidationSnapshot
    {
        uint32_t width = 0;
        uint32_t height = 0;
        std::vector<GpuFloat4> hdr{};
        std::vector<GpuSurface> surfaces{};
        std::vector<GpuReservoirSample> reservoirSamples{};
        std::vector<GpuReservoirStats> reservoirStats{};
        std::vector<GpuAcceptance> acceptance{};
    };

    class RenderPipeline final : public IRenderPipeline
    {
    public:
        explicit RenderPipeline(bool enableUI = true);
        ~RenderPipeline() override;

        void Render(GraphicsRenderer& renderer, float deltaTime) override;
        void RenderUI(GraphicsRenderer& renderer) override;
        void OnResizeFrameBuffer(GraphicsRenderer& renderer, uint32_t width, uint32_t height) override;
        void OnResizeRenderTexture(GraphicsRenderer& renderer, uint32_t width, uint32_t height) override;

        Settings& GetSettings() noexcept { return m_Settings; }
        const Settings& GetSettings() const noexcept { return m_Settings; }
        uint64_t GetRenderedFrameCount() const noexcept { return m_RenderedFrameCount; }
        uint64_t GetUIFrameCount() const noexcept { return m_UIFrameCount; }
        bool IsInitialized() const noexcept;
        bool IsDXRAvailable() const noexcept;
        const std::string& GetLastError() const noexcept;

        void ResetHistory() noexcept;
        bool LoadEnvironmentHDR(const std::filesystem::path& filename);
        bool LoadDefaultEnvironment();
        bool CaptureValidation(GraphicsRenderer& renderer, ValidationSnapshot& snapshot, std::string& error);

    private:
        struct Implementation;
        std::unique_ptr<Implementation> m_Implementation{};
        Settings m_Settings{};
        bool m_EnableUI = true;
        uint64_t m_RenderedFrameCount = 0;
        uint64_t m_UIFrameCount = 0;
    };

}
