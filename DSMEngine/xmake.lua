targetName = "DSMEngine"
target(targetName)
    set_kind("static")
    set_targetdir(path.join(binDir, targetName))

    add_deps("spdlog")
    add_deps("imgui")
    add_includedirs("./",{public = true})
    add_files("**.cpp")
    add_headerfiles("**.h")
    --add_headerfiles("Shaders/**.hlsli", "Shaders/**.hlsl")

target_end()