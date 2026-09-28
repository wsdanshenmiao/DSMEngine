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
binDir = path.join(os.projectdir(), "bin", string.lower(configName))
engineBinDir = path.join(os.projectdir(), "build", "engine", configName)
thirdPartyBinDir = path.join(os.projectdir(), "build", "thirdparty", configName)

if is_mode("debug") then
    set_symbols("debug")
    set_optimize("none")
end

-- 添加系统依赖库。
add_syslinks("d3d12", "dxgi", "d3dcompiler", "dxguid", "user32", "comdlg32")

includes("rules.lua")
includes("Engine/ThirdParty")
includes("Engine")
includes("Projects/PBR")
includes("Projects/RayTracing")
includes("Tests")
