set_project("DSMEngine")

if is_os("windows") then
    add_defines("UNICODE")
    add_defines("_UNICODE")
    add_defines("SPDLOG_WCHAR_TO_UTF8_SUPPORT")
    add_defines("SPDLOG_UTF8_TO_WCHAR_CONSOLE")
end

add_rules("mode.debug", "mode.release")
set_languages("c11", "cxx23")
set_toolchains("msvc")
set_encodings("utf-8")

local configName = is_mode("debug") and "Debug" or "Release"
binDir = path.join(os.projectdir(), "bin", string.lower(configName))/
engineBinDir = path.join(os.projectdir(), "build", "engine", configName)
thirdPartyBinDir = path.join(os.projectdir(), "build", "thirdparty", configName)

if is_mode("debug") then
    set_symbols("debug")
    set_optimize("none")
end
-- 自定义构建规则

-- 添加系统依赖库。
add_syslinks("d3d12", "dxgi", "d3dcompiler", "dxguid", "user32", "comdlg32")

-- 复制 ImGui 的 UI 配置文件。
rule("Imguiini")
    after_build(function(target)
        local source = target:extraconf("rules", "Imguiini", "source") or "imgui.ini"
        local sourceFile = path.join(target:scriptdir(), source)
        local destinationFile = path.join(target:targetdir(), "imgui.ini")
        -- 源文件只提供首次运行的默认布局，不能覆盖用户已经保存的布局。
        if os.isfile(sourceFile) and not os.isfile(destinationFile) then
            os.cp(sourceFile, destinationFile)
        end
    end)
rule_end()

-- DXC 运行库（dxcompiler.dll / dxil.dll）改由 xmake 包 directxshadercompiler 提供，
-- 见 Engine/ThirdParty/xmake.lua。包只提供库与头文件，构建期不再向输出目录复制 DLL。

includes("Engine/ThirdParty")
includes("Engine")
includes("Projects/PBR")
includes("Projects/RayTracing")
includes("Tests")
