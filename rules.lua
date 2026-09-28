-- 自定义构建规则

-- 复制 ImGui 的 UI 配置文件。
rule("Imguiini")
    after_build(function(target)
        local source = target:extraconf("rules", "Imguiini", "source") or "imgui.ini"
        local sourceFile = path.join(target:scriptdir(), source)
        local destinationFile = path.join(target:targetdir(), "imgui.ini")
        -- 源文件只提供首次运行的默认布局，不能覆盖用户已经保存的布局。
        if os.isfile(sourceFile) and not os.isfile(destinationFile) then
            os.cp(sourceFile, destinationFile)
        end
    end)
rule_end()

-- 从本机 Windows SDK 复制 DXC 运行库，不依赖旧 bin 目录或用户包缓存。
rule("DXCRuntimeCopy")
    after_build(function(target)
        local sdkRoot = path.join(os.getenv("ProgramFiles(x86)") or "C:/Program Files (x86)", "Windows Kits", "10", "bin")
        local candidates = os.files(path.join(sdkRoot, "*/x64/dxcompiler.dll"))
        table.sort(candidates)
        local compiler = candidates[#candidates]
        assert(compiler, "未找到 Windows SDK x64/dxcompiler.dll")
        local dxil = path.join(path.directory(compiler), "dxil.dll")
        assert(os.isfile(dxil), "未找到与 dxcompiler.dll 同版本的 dxil.dll: " .. dxil)
        os.cp(compiler, target:targetdir(), {copy_if_different = true})
        os.cp(dxil, target:targetdir(), {copy_if_different = true})
    end)
rule_end()