-- 自定义规则

-- 复制 Imgui 的UI配置文件
rule("Imguiini")
    -- 在构建之后执行
    after_build(
        function(target)
            iniFile = path.join(target:scriptdir(), "imgui.ini")
            -- 判断指定文件是否存在
            if os.isfile(iniFile) then
                -- 将imgui的GUI配置文件复制到目标处
                os.cp(iniFile, target:targetdir())
            end
        end)
rule_end()

-- 将 Shader 文件复制到指定路径
rule("ShaderCopy")
    -- 设置规制支持的文件扩展类型
    set_extensions(".hlsl", ".hlsli")
    after_build(
        function(target)
            shaderFiles = path.join(target:scriptdir(), "/Shaders");
            -- 判断文件是否存在
            if(os.exists(shaderFiles)) then
                os.cp(shaderFiles, target:targetdir())
            end
        end)
rule_end()

rule("EngineShderCopy")
    set_extensions(".hlsl", ".hlsli")
    after_build(
        function(target)
            shaderFiles = path.join(target:scriptdir(), "../../DSMEngine/Shaders");
            -- 判断文件是否存在
            if(os.exists(shaderFiles)) then
                os.cp(shaderFiles, target:targetdir())
            end
        end)
rule_end()

rule("ModelCopy")
    after_build(
        function(target)
            modelFiles = path.join(target:scriptdir(), "../Models")
            if(os.exists(modelFiles)) then
                os.cp(modelFiles, target:targetdir())
            end
        end)
rule_end()

rule("TextureCopy")
    after_build(
        function(target)
            modelFiles = path.join(target:scriptdir(), "../Textures")
            if(os.exists(modelFiles)) then
                os.cp(modelFiles, target:targetdir())
            end
        end)
rule_end()