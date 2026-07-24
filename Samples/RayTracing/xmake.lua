target("RayTracing")
    set_kind("binary")
    set_targetdir(path.join(binDir, "RayTracing"))

    add_deps("DSMEngine")
    add_files("main.cpp")
    add_rules("ShaderCopy")
target_end()
