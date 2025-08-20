#include "ShaderCompiler.h"
#include "Utils/Utils.h"
#include "Core/Core.h"
#include <wrl/client.h>
#include <cassert>
#include <format>

using Microsoft::WRL::ComPtr;

namespace DSM {

    class ShaderCompiler
    {
    public:
        ShaderCompiler()
        {
            DSM_CORE_ASSERT(DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(m_DxcUtils.GetAddressOf())));
            DSM_CORE_ASSERT(DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(m_DxcCompiler.GetAddressOf())));
        }
        ~ShaderCompiler() = default;
        ShaderCompiler(const ShaderCompiler&) = delete;
        ShaderCompiler(ShaderCompiler&&) = delete;

        ComPtr<IDxcBlob> CompilerShader(
            const std::wstring& fileName,
            const std::wstring& entryPoint,
            const std::wstring& target,
            const std::vector<DxcDefine>& defines)
        {
            ComPtr<IDxcIncludeHandler> includeHandler{};
            DSM_CORE_ASSERT(m_DxcUtils->CreateDefaultIncludeHandler(includeHandler.GetAddressOf()));

            ComPtr<IDxcCompilerArgs> compilerArgs{};
            DSM_CORE_ASSERT(m_DxcUtils->BuildArguments(
                fileName.c_str(),
                entryPoint.c_str(),
                target.c_str(),
                nullptr,
                0,
                defines.size() == 0 ? nullptr : defines.data(),
                defines.size(),
                compilerArgs.GetAddressOf()));

            ComPtr<IDxcBlobEncoding> sourceFileEncoding{};
            DSM_CORE_ASSERT(m_DxcUtils->LoadFile(fileName.c_str(), nullptr, sourceFileEncoding.GetAddressOf()));

            DxcBuffer sourceBuffer{};
            sourceBuffer.Ptr = sourceFileEncoding->GetBufferPointer();
            sourceBuffer.Size = sourceFileEncoding->GetBufferSize();
            sourceBuffer.Encoding = DXC_CP_ACP;

            ComPtr<IDxcResult> result{};
            DSM_CORE_ASSERT(m_DxcCompiler->Compile(
                &sourceBuffer,
                compilerArgs->GetArguments(),
                compilerArgs->GetCount(),
                includeHandler.Get(),
                IID_PPV_ARGS(result.GetAddressOf())));

            ComPtr<IDxcBlobUtf8> pErrors = nullptr;
            DSM_CORE_ASSERT(result->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(pErrors.GetAddressOf()), nullptr));

            auto errorInfo = pErrors->GetStringPointer();
            if(pErrors != nullptr && pErrors->GetStringLength() != 0){
                DSM_CORE_ERROR("Shader Compile Fail: {}\n", errorInfo);
            }
            
            ComPtr<IDxcBlob> shaderByteCode = nullptr;
            ComPtr<IDxcBlobUtf16> pShaderName = nullptr;
            DSM_CORE_ASSERT(result->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&shaderByteCode), &pShaderName));
            
            return shaderByteCode;
        }

    private:
        ComPtr<IDxcUtils> m_DxcUtils;
        ComPtr<IDxcCompiler3> m_DxcCompiler;
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
    
    ShaderByteCode::ShaderByteCode(const ShaderDesc& shaderDesc)
    {
        std::wstring fileName = Utility::UTF8ToWString(shaderDesc.m_FileName);
        std::wstring enterPoint = Utility::UTF8ToWString(shaderDesc.m_EnterPoint);
        std::wstring target = GetComileTarget(shaderDesc.m_Type, shaderDesc.m_Mode);
        auto defines = shaderDesc.m_Defines.Finish();
        
        ComPtr<IDxcBlob> shaderByteCode = s_ShaderCompiler.CompilerShader(fileName, enterPoint, target, defines);
        m_ByteCode.resize(shaderByteCode->GetBufferSize());
        memcpy(m_ByteCode.data(), shaderByteCode->GetBufferPointer(), shaderByteCode->GetBufferSize());
    }
}
