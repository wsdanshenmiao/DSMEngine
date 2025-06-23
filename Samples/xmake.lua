targetName = "Sample"
target(targetName)
    set_kind("binary")
    set_targetdir(path.join(binDir, targetName))

    add_deps("DSMEngine")

    add_files("**.cpp")
    add_headerfiles("**.h")

target_end()