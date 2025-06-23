set_project("DSMEngine")

if is_os("windows") then 
    add_defines("UNICODE")
    add_defines("_UNICODE")
end

add_rules("mode.debug", "mode.release")
set_languages("c11", "cxx23")
set_toolchains("msvc")
set_encodings("utf-8")

if is_mode("debug") then 
    binDir = path.join(os.projectdir(), "bin/debug/")
else 
    binDir = path.join(os.projectdir(), "bin/release/")
end 

-- 添加系统依赖库
add_syslinks("d3d12", "dxgi", "d3dcompiler", "dxguid", "user32")


includes("rules.lua")
includes("ThirdParty")
includes("DSMEngine")

includes("Samples")