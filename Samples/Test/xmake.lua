targetName = "Test"
target(targetName)
    set_kind("binary")
    set_targetdir(path.join(binDir, targetName))

    add_deps("DSMEngine")

    add_files("**.cpp")
    add_headerfiles("**.h")

    add_rules("Imguiini")
    add_rules("ShaderCopy")
    add_rules("ModelCopy")
    add_rules("TextureCopy")
target_end()