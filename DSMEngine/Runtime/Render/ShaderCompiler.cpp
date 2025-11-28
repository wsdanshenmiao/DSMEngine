#include "ShaderCompiler.h"
#include "Runtime/Utils/Utils.h"
#include "Runtime/Core/Macro.h"
#include "Runtime/Graphics/D3D12/D3D12Common.h"

#include <wrl/client.h>
#include <cassert>
#include <format>

namespace DSM {

    inline void AssertShaderCompiler(HRESULT hr)
    {
        if(FAILED(hr)){
            DSM_CORE_ERROR("Compiler shader error.Error msg: {}", GetHRErrorMessage(hr));
        }
    }

    class ShaderCompiler
    {
    public:
        ShaderCompiler()
        {
            AssertShaderCompiler(DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(m_DxcUtils.GetAddressOf())));
            AssertShaderCompiler(DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(m_DxcCompiler.GetAddressOf())));
        }
        ~ShaderCompiler() = default;
        ShaderCompiler(const ShaderCompiler&) = delete;
        ShaderCompiler(ShaderCompiler&&) = delete;
        ShaderCompiler& operator=(const ShaderCompiler&) = delete;
        ShaderCompiler& operator=(ShaderCompiler&&) = delete;

        RefPtr<IDxcBlob> CompilerShader(
            const std::wstring& fileName,
            const std::wstring& entryPoint,
            const std::wstring& target,
            const std::vector<DxcDefine>& defines)
        {
            RefPtr<IDxcIncludeHandler> includeHandler{};
            AssertShaderCompiler(m_DxcUtils->CreateDefaultIncludeHandler(includeHandler.GetAddressOf()));

            std::array<const wchar_t*, 5> args = {
                L"-Zi",          // 调试信息
                L"-Qembed_debug", // 嵌入调试信息到 DXIL
                L"-Od",          // 禁用优化
                L"-WX",          // 警告视为错误
                L"-Ges",         // 启用严格模式
            };

            RefPtr<IDxcCompilerArgs> compilerArgs{};
            AssertShaderCompiler(m_DxcUtils->BuildArguments(
                fileName.c_str(),
                entryPoint.c_str(),
                target.c_str(),
                args.size() == 0 ? nullptr : args.data(),
                args.size(),
                defines.size() == 0 ? nullptr : defines.data(),
                defines.size(),
                compilerArgs.GetAddressOf()));

            RefPtr<IDxcBlobEncoding> sourceFileEncoding{};
            AssertShaderCompiler(m_DxcUtils->LoadFile(fileName.c_str(), nullptr, sourceFileEncoding.GetAddressOf()));

            DxcBuffer sourceBuffer{};
            sourceBuffer.Ptr = sourceFileEncoding->GetBufferPointer();
            sourceBuffer.Size = sourceFileEncoding->GetBufferSize();
            sourceBuffer.Encoding = DXC_CP_ACP;

            RefPtr<IDxcResult> result{};
            AssertShaderCompiler(m_DxcCompiler->Compile(
                &sourceBuffer,
                compilerArgs->GetArguments(),
                compilerArgs->GetCount(),
                includeHandler.Get(),
                IID_PPV_ARGS(result.GetAddressOf())));

            RefPtr<IDxcBlobUtf8> pErrors = nullptr;
            AssertShaderCompiler(result->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(pErrors.GetAddressOf()), nullptr));

            auto errorInfo = pErrors->GetStringPointer();
            if(pErrors != nullptr && pErrors->GetStringLength() != 0){
                DSM_CORE_ERROR("Shader Compile Fail: {}\n", errorInfo);
            }
            
            RefPtr<IDxcBlob> shaderByteCode = nullptr;
            RefPtr<IDxcBlobUtf16> pShaderName = nullptr;
            AssertShaderCompiler(result->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&shaderByteCode), &pShaderName));
            
            return shaderByteCode;
        }

    private:
        RefPtr<IDxcUtils> m_DxcUtils;
        RefPtr<IDxcCompiler3> m_DxcCompiler;
    };

    static ShaderCompiler s_ShaderCompiler{};


    inline constexpr std::wstring GetComileTarget(ShaderType type, ShaderMode mode)
    {
        std::wstring target = L"";
        switch (type) {
            case ShaderType::Vertex: target += L"vs_"; break;
            case ShaderType::Hull: target += L"hs_"; break;
            case ShaderType::Domain: target += L"ds_"; break;
            case ShaderType::Geometry: target += L"gs_"; break;
            case ShaderType::Pixel: target += L"ps_"; break;
            case ShaderType::Compute: target += L"cs_"; break;
            case ShaderType::Mesh: target += L"ms_"; break;
            case ShaderType::Amplification: target += L"as_"; break;
            default: assert("Invalid ShaderType"); break;
        }
        switch (mode) {
            case ShaderMode::SM_6_0: target += L"6_0"; break;
            case ShaderMode::SM_6_1: target += L"6_1"; break;
            case ShaderMode::SM_6_2: target += L"6_2"; break;
            case ShaderMode::SM_6_3: target += L"6_3"; break;
            case ShaderMode::SM_6_4: target += L"6_4"; break;
            case ShaderMode::SM_6_5: target += L"6_5"; break;
            case ShaderMode::SM_6_6: target += L"6_6"; break;
            case ShaderMode::SM_6_7: target += L"6_7"; break;
            case ShaderMode::SM_6_8: target += L"6_8"; break;
            default: assert("Invalid ShaderMode"); break;
        }
        return target;
    }
    
    ShaderByteCode::ShaderByteCode(const ShaderCompileDesc& shaderDesc)
        :m_Desc(shaderDesc)    
    {
        std::wstring fileName = Utility::UTF8ToWString(shaderDesc.fileName);
        std::wstring enterPoint = Utility::UTF8ToWString(shaderDesc.enterPoint);
        std::wstring target = GetComileTarget(shaderDesc.type, shaderDesc.mode);
        auto defines = shaderDesc.defines.Finish();
        
        RefPtr<IDxcBlob> shaderByteCode = s_ShaderCompiler.CompilerShader(fileName, enterPoint, target, defines);
        m_ByteCode.resize(shaderByteCode->GetBufferSize());
        memcpy(m_ByteCode.data(), shaderByteCode->GetBufferPointer(), shaderByteCode->GetBufferSize());
    }
}
