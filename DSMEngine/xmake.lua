targetName = "DSMEngine"
target(targetName)
    set_kind("static")
    set_targetdir(path.join(binDir, targetName))

    add_deps("spdlog")
    add_deps("imgui")
    add_deps("glfw")
    add_deps("dxc")
    add_links("dxcompiler")

    add_includedirs("./",{public = true})
    add_files("**.cpp")
    add_headerfiles("**.h")

target_end()