#include "RestirDIEnvironment.h"

#include <stb_image.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>

namespace DSM::RestirDI {
    namespace {
        struct CubeFace
        {
            int width = 0;
            int height = 0;
            std::vector<uint8_t> pixels{};
        };

        float SrgbToLinear(float value)
        {
            return value <= 0.04045f
                ? value / 12.92f
                : std::pow((value + 0.055f) / 1.055f, 2.4f);
        }

        GpuFloat4 SampleFace(const CubeFace& face, float u, float v)
        {
            const float pixelX = std::clamp((u * 0.5f + 0.5f) * face.width - 0.5f,
                0.0f, static_cast<float>(face.width - 1));
            const float pixelY = std::clamp((v * 0.5f + 0.5f) * face.height - 0.5f,
                0.0f, static_cast<float>(face.height - 1));
            const int x0 = static_cast<int>(pixelX);
            const int y0 = static_cast<int>(pixelY);
            const int x1 = std::min(x0 + 1, face.width - 1);
            const int y1 = std::min(y0 + 1, face.height - 1);
            const float tx = pixelX - x0;
            const float ty = pixelY - y0;

            auto load = [&face](int x, int y) {
                const uint8_t* texel = face.pixels.data() + (size_t(y) * face.width + x) * 4;
                return GpuFloat4{
                    SrgbToLinear(texel[0] / 255.0f),
                    SrgbToLinear(texel[1] / 255.0f),
                    SrgbToLinear(texel[2] / 255.0f),
                    texel[3] / 255.0f};
            };
            const GpuFloat4 c00 = load(x0, y0);
            const GpuFloat4 c10 = load(x1, y0);
            const GpuFloat4 c01 = load(x0, y1);
            const GpuFloat4 c11 = load(x1, y1);
            auto interpolate = [=](float GpuFloat4::*member) {
                const float top = c00.*member + (c10.*member - c00.*member) * tx;
                const float bottom = c01.*member + (c11.*member - c01.*member) * tx;
                return top + (bottom - top) * ty;
            };
            return {
                interpolate(&GpuFloat4::x), interpolate(&GpuFloat4::y),
                interpolate(&GpuFloat4::z), interpolate(&GpuFloat4::w)};
        }

        GpuFloat4 SampleCube(const std::array<CubeFace, 6>& faces, float x, float y, float z)
        {
            const float ax = std::abs(x);
            const float ay = std::abs(y);
            const float az = std::abs(z);
            uint32_t face = 0;
            float u = 0.0f;
            float v = 0.0f;
            if (ax >= ay && ax >= az) {
                if (x >= 0.0f) { face = 0; u = -z / ax; v = -y / ax; }
                else { face = 1; u = z / ax; v = -y / ax; }
            }
            else if (ay >= ax && ay >= az) {
                if (y >= 0.0f) { face = 2; u = x / ay; v = z / ay; }
                else { face = 3; u = x / ay; v = -z / ay; }
            }
            else {
                if (z >= 0.0f) { face = 4; u = x / az; v = -y / az; }
                else { face = 5; u = -x / az; v = -y / az; }
            }
            return SampleFace(faces[face], u, v);
        }

        void BuildEnvironmentAlias(EnvironmentData& environment)
        {
            std::vector<float> weights(environment.pixels.size());
            const float dPhi = 2.0f * std::numbers::pi_v<float> / environment.width;
            for (uint32_t y = 0; y < environment.height; ++y) {
                const float theta0 = std::numbers::pi_v<float> * y / environment.height;
                const float theta1 = std::numbers::pi_v<float> * (y + 1) / environment.height;
                const float solidAngle = dPhi * (std::cos(theta0) - std::cos(theta1));
                for (uint32_t x = 0; x < environment.width; ++x) {
                    const auto& color = environment.pixels[size_t(y) * environment.width + x];
                    const float luminance = std::max(
                        0.2126f * color.x + 0.7152f * color.y + 0.0722f * color.z, 0.0f);
                    weights[size_t(y) * environment.width + x] = luminance * solidAngle;
                }
            }
            environment.aliasTable = BuildAliasTable(weights);
        }
    }

    bool LoadDaylightEnvironment(
        const std::filesystem::path& assetsDirectory,
        EnvironmentData& output,
        std::string& error)
    {
        std::array<CubeFace, 6> faces{};
        for (uint32_t faceIndex = 0; faceIndex < faces.size(); ++faceIndex) {
            const auto filename = assetsDirectory / "Textures" /
                ("daylight" + std::to_string(faceIndex) + ".png");
            int componentCount = 0;
            stbi_uc* data = stbi_load(filename.string().c_str(),
                &faces[faceIndex].width, &faces[faceIndex].height, &componentCount, 4);
            if (data == nullptr || faces[faceIndex].width <= 0 || faces[faceIndex].height <= 0) {
                if (data != nullptr) stbi_image_free(data);
                error = "无法加载默认环境面：" + filename.string();
                return false;
            }
            const size_t byteCount = size_t(faces[faceIndex].width) * faces[faceIndex].height * 4;
            faces[faceIndex].pixels.assign(data, data + byteCount);
            stbi_image_free(data);
        }

        EnvironmentData candidate{};
        candidate.source = EnvironmentSource::DaylightCube;
        candidate.width = 512;
        candidate.height = 256;
        candidate.sourcePath = assetsDirectory;
        candidate.pixels.resize(size_t(candidate.width) * candidate.height);
        for (uint32_t y = 0; y < candidate.height; ++y) {
            const float theta = std::numbers::pi_v<float> * (y + 0.5f) / candidate.height;
            for (uint32_t x = 0; x < candidate.width; ++x) {
                const float phi = 2.0f * std::numbers::pi_v<float> *
                    ((x + 0.5f) / candidate.width - 0.5f);
                const float sinTheta = std::sin(theta);
                candidate.pixels[size_t(y) * candidate.width + x] = SampleCube(
                    faces, sinTheta * std::cos(phi), std::cos(theta), sinTheta * std::sin(phi));
            }
        }
        BuildEnvironmentAlias(candidate);
        output = std::move(candidate);
        error.clear();
        return true;
    }

    bool LoadRadianceEnvironment(
        const std::filesystem::path& filename,
        EnvironmentData& output,
        std::string& error)
    {
        int width = 0;
        int height = 0;
        int componentCount = 0;
        float* data = stbi_loadf(filename.string().c_str(), &width, &height, &componentCount, 4);
        if (data == nullptr || width <= 0 || height <= 0) {
            if (data != nullptr) stbi_image_free(data);
            error = "无法加载 Radiance HDR：" + filename.string();
            return false;
        }

        EnvironmentData candidate{};
        candidate.source = EnvironmentSource::RadianceHDR;
        candidate.width = static_cast<uint32_t>(width);
        candidate.height = static_cast<uint32_t>(height);
        candidate.sourcePath = filename;
        candidate.pixels.resize(size_t(width) * height);
        for (size_t index = 0; index < candidate.pixels.size(); ++index) {
            candidate.pixels[index] = {
                std::isfinite(data[index * 4]) ? std::max(data[index * 4], 0.0f) : 0.0f,
                std::isfinite(data[index * 4 + 1]) ? std::max(data[index * 4 + 1], 0.0f) : 0.0f,
                std::isfinite(data[index * 4 + 2]) ? std::max(data[index * 4 + 2], 0.0f) : 0.0f,
                1.0f};
        }
        stbi_image_free(data);
        BuildEnvironmentAlias(candidate);
        output = std::move(candidate);
        error.clear();
        return true;
    }

}
