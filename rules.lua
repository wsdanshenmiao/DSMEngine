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
            local destination = path.join(target:targetdir(), "Assets")
            if os.exists(assetsFiles) and not os.isdir(destination) then
                os.cp(assetsFiles, target:targetdir())
            end
        end)
rule_end()

-- 将 ShaderCompiler 运行所需的 DXC DLL 放到可执行文件旁边。
rule("DXCRuntimeCopy")
    after_build(
        function(target)
            local programFilesX86 = os.getenv("ProgramFiles(x86)")
            local dxcRuntimeDir = nil
            if programFilesX86 then
                local sdkRoot = path.join(programFilesX86, "Windows Kits", "10")
                local sdkBinaryDirs = os.dirs(path.join(sdkRoot, "bin", "*", "x64"))
                table.sort(sdkBinaryDirs)
                for index = #sdkBinaryDirs, 1, -1 do
                    local candidate = sdkBinaryDirs[index]
                    if os.isfile(path.join(candidate, "dxcompiler.dll")) and
                       os.isfile(path.join(candidate, "dxil.dll")) then
                        dxcRuntimeDir = candidate
                        break
                    end
                end
                if not dxcRuntimeDir then
                    local candidate = path.join(sdkRoot, "Redist", "D3D", "x64")
                    if os.isfile(path.join(candidate, "dxcompiler.dll")) and
                       os.isfile(path.join(candidate, "dxil.dll")) then
                        dxcRuntimeDir = candidate
                    end
                end
            end

            assert(dxcRuntimeDir, target:name() .. " 需要 Windows SDK 中的 dxcompiler.dll 和 dxil.dll")
            os.cp(path.join(dxcRuntimeDir, "dxcompiler.dll"), target:targetdir())
            os.cp(path.join(dxcRuntimeDir, "dxil.dll"), target:targetdir())
        end)
rule_end()
