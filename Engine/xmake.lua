target("DSMEngine")
    set_kind("static")
    set_targetdir(engineBinDir)

    add_deps("EngineThirdParty")
    add_packages("assimp")
    -- 提供 <dxcapi.h>、dxcompiler.lib，并让 dxcompiler.dll / dxil.dll 进入运行环境。
    add_packages("directxshadercompiler")
    add_links("dxcompiler")

    add_includedirs(path.join(os.projectdir(), "Engine", "Source"), {public = true})
    add_includedirs(path.join(os.projectdir(), "Engine"), {public = true})
    add_files("Source/Runtime/**.cpp")
    add_headerfiles("Source/Runtime/**.h")
target_end()

target("DSMEditor")
    set_kind("static")
    set_targetdir(engineBinDir)

    add_deps("DSMEngine", "EngineThirdParty")
    add_includedirs(path.join(os.projectdir(), "Engine", "Source"), {public = true})
    add_includedirs(path.join(os.projectdir(), "Engine"), {public = true})
    add_files("Source/Editor/**.cpp")
    add_headerfiles("Source/Editor/**.h")
target_end()
