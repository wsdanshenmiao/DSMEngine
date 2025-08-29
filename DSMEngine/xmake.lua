targetName = "DSMEngine"
target(targetName)
    set_kind("static")
    set_targetdir(path.join(binDir, targetName))

    add_deps("ThirdParty")
    
    add_packages("assimp")

    add_links("dxcompiler")

    add_includedirs("./",{public = true})
    add_files("**.cpp")
    add_headerfiles("**.h")

target_end()