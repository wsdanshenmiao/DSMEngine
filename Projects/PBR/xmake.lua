target("PBR")
    set_kind("binary")
    set_targetdir(path.join(os.projectdir(), "Projects", "PBR", "Binaries", is_mode("debug") and "Debug" or "Release"))

    add_deps("DSMEngine", "DSMEditor")
    add_includedirs(path.join(os.projectdir(), "Engine", "Source"), {public = true})
    add_includedirs(path.join(os.projectdir(), "Engine"), {public = true})
    add_files("Source/PBR/**.cpp")
    add_headerfiles("Source/PBR/**.h")

    add_rules("Imguiini")
    add_rules("DXCRuntimeCopy")
target_end()
