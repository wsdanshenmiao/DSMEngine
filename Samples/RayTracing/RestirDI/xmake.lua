target("RestirDI")
    set_kind("binary")
    set_targetdir(path.join(binDir, "RestirDI"))

    add_deps("DSMEngine")
    add_files("**.cpp")
    add_headerfiles("**.h")

    add_rules("Imguiini")
    add_rules("ShaderCopy")

    after_build(function(target)
        local projectRoot = os.projectdir()
        local targetDir = target:targetdir()
        local engineShaders = path.join(projectRoot, "DSMEngine", "Shaders")
        local sampleAssets = path.join(projectRoot, "Samples", "Assets")

        if os.exists(engineShaders) then
            os.cp(engineShaders, targetDir)
        end
        -- Assets 体积接近 1 GiB；已有完整副本时不在每次增量链接后重复复制。
        if os.exists(sampleAssets) and not os.isdir(path.join(targetDir, "Assets")) then
            os.cp(sampleAssets, targetDir)
        end

        -- DXC 是 DSMEngine 的运行时依赖，Windows SDK 不保证将其放进 PATH。
        local programFilesX86 = os.getenv("ProgramFiles(x86)")
        local dxcRuntimeDir = nil
        if programFilesX86 then
            local sdkRoot = path.join(programFilesX86, "Windows Kits", "10")
            local sdkBinaryDirs = os.dirs(path.join(sdkRoot, "bin", "*", "x64"))
            table.sort(sdkBinaryDirs)
            for index = #sdkBinaryDirs, 1, -1 do
                if os.isfile(path.join(sdkBinaryDirs[index], "dxcompiler.dll")) and
                   os.isfile(path.join(sdkBinaryDirs[index], "dxil.dll")) then
                    dxcRuntimeDir = sdkBinaryDirs[index]
                    break
                end
            end
            if not dxcRuntimeDir then
                local redistDir = path.join(sdkRoot, "Redist", "D3D", "x64")
                if os.isfile(path.join(redistDir, "dxcompiler.dll")) and
                   os.isfile(path.join(redistDir, "dxil.dll")) then
                    dxcRuntimeDir = redistDir
                end
            end
        end
        assert(dxcRuntimeDir, "RestirDI 需要 Windows SDK 中的 dxcompiler.dll 和 dxil.dll")
        os.cp(path.join(dxcRuntimeDir, "dxcompiler.dll"), targetDir)
        os.cp(path.join(dxcRuntimeDir, "dxil.dll"), targetDir)
    end)
target_end()
