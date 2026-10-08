target("RayTracing")
    set_kind("binary")
    set_targetdir(path.join(os.projectdir(), "Projects", "RayTracing", "Binaries", is_mode("debug") and "Debug" or "Release"))

    add_deps("DSMEngine", "DSMEditor")
    add_includedirs(path.join(os.projectdir(), "Engine", "Source"), {public = true})
    add_includedirs(path.join(os.projectdir(), "Engine"), {public = true})
    add_files("Source/RayTracing/**.cpp")
    add_headerfiles("Source/RayTracing/**.h")

    add_rules("Imguiini")
target_end()
