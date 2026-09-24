targetName = "RayTracing"
target(targetName)
    set_kind("binary")
    set_targetdir(path.join(binDir, targetName))

    add_deps("DSMEngine")
    add_files("**.cpp")
    add_headerfiles("**.h")

    add_rules("Imguiini")
    add_rules("ShaderCopy", {source = "RestirDI/Shaders"})
    add_rules("EngineShaderCopy")
    add_rules("AssetsCopy")
target_end()
