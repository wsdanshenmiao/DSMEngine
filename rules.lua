-- 自定义规则

-- 复制 Imgui 的UI配置文件
rule("Imguiini")
    after_build(
        function(target)
            local source = target:extraconf("rules", "Imguiini", "source") or "imgui.ini"
            local sourceFile = path.join(target:scriptdir(), source)
            local destinationFile = path.join(target:targetdir(), "imgui.ini")
            -- 源文件只提供首次运行的默认布局，不能覆盖用户已经保存的布局。
            if os.isfile(sourceFile) and not os.isfile(destinationFile) then
                os.cp(sourceFile, destinationFile)
            end
        end)
rule_end()

-- 将 Shader 文件复制到指定路径
rule("ShaderCopy")
    -- 设置规制支持的文件扩展类型
    set_extensions(".hlsl", ".hlsli")
    after_build(
        function(target)
            local source = target:extraconf("rules", "ShaderCopy", "source") or "Shaders"
            local shaderFiles = path.join(target:scriptdir(), source)
            if os.exists(shaderFiles) then
                os.cp(shaderFiles, target:targetdir())
            end
        end)
rule_end()

rule("EngineShaderCopy")
    set_extensions(".hlsl", ".hlsli")
    after_build(
        function(target)
            local shaderFiles = path.join(os.projectdir(), "DSMEngine", "Shaders")
            if os.exists(shaderFiles) then
                os.cp(shaderFiles, target:targetdir())
            end
        end)
rule_end()

rule("AssetsCopy")
    after_build(
        function(target)
            local assetsFiles = path.join(os.projectdir(), "Samples", "Assets")
            if os.isdir(assetsFiles) then
                -- 目标目录可能已经存在；按文件同步变更，避免构建后继续使用旧资产。
                os.cp(assetsFiles, target:targetdir(), {copy_if_different = true})
            end
        end)
rule_end()
