-- Engine-owned 第三方依赖的本地构建定义。
-- 供应商源码位于当前脚本目录，构建不解析系统包或用户缓存中的其他版本。

local thirdPartyDir = os.scriptdir()

package("assimp")
    add_deps("cmake")
    set_sourcedir(path.join(thirdPartyDir, "assimp"))

    on_install(function(package)
        local configs = {
            "-DBUILD_SHARED_LIBS=OFF",
            "-DASSIMP_BUILD_ASSIMP_TOOLS=OFF",
            "-DASSIMP_BUILD_SAMPLES=OFF",
            "-DASSIMP_BUILD_TESTS=OFF",
            "-DASSIMP_BUILD_DOCS=OFF",
            "-DASSIMP_BUILD_FRAMEWORK=OFF",
            "-DASSIMP_BUILD_ZLIB=ON",
            "-DASSIMP_NO_EXPORT=ON",
            "-DASSIMP_INSTALL=ON",
            "-DASSIMP_WARNINGS_AS_ERRORS=OFF",
            "-DASSIMP_INJECT_DEBUG_POSTFIX=OFF",
            "-DCMAKE_BUILD_TYPE=" .. (package:debug() and "Debug" or "Release")
        }
        import("package.tools.cmake").install(package, configs)
    end)
package_end()

add_requires("assimp", {system = false})

-- DXC（dxcompiler.dll / dxil.dll / dxcompiler.lib / dxcapi.h）改用 xmake-repo 的官方包，
-- 直接使用微软发布的 dxc dev kit，版本锁定为 1.8.2405。
-- 这样构建不再依赖本机 Windows SDK 的目录布局（SDK 各版本 bin 目录并不保证携带 dxil.dll），
-- 同时替代了原先 after_build 里手动复制 DXC 运行库的规则。
add_requires("directxshadercompiler")

target("EngineThirdParty")
    set_kind("static")
    set_targetdir(thirdPartyBinDir)

    add_headerfiles(path.join(thirdPartyDir, "spdlog/include/**.h"))
    add_includedirs(path.join(thirdPartyDir, "spdlog/include"), {public = true})

    add_headerfiles(path.join(thirdPartyDir, "glfw/include/GLFW/*.h"))
    add_files(path.join(thirdPartyDir, "glfw/src/*.c"))
    add_includedirs(path.join(thirdPartyDir, "glfw/include"), {public = true})
    if is_plat("windows") then
        add_defines("_GLFW_WIN32")
        add_syslinks("gdi32", "shell32")
    end

    add_headerfiles(
        path.join(thirdPartyDir, "imgui/*.h"),
        path.join(thirdPartyDir, "imgui/backends/imgui_impl_dx12.h"),
        path.join(thirdPartyDir, "imgui/backends/imgui_impl_glfw.h"),
        path.join(thirdPartyDir, "imgui/backends/imgui_impl_win32.h"))
    add_files(
        path.join(thirdPartyDir, "imgui/*.cpp"),
        path.join(thirdPartyDir, "imgui/backends/imgui_impl_dx12.cpp"),
        path.join(thirdPartyDir, "imgui/backends/imgui_impl_glfw.cpp"),
        path.join(thirdPartyDir, "imgui/backends/imgui_impl_win32.cpp"))
    add_includedirs(path.join(thirdPartyDir, "imgui"), {public = true})

    add_headerfiles(path.join(thirdPartyDir, "stb_image/**.h"))
    add_includedirs(path.join(thirdPartyDir, "stb_image"), {public = true})

    add_headerfiles(path.join(thirdPartyDir, "DDSTextureLoader/**.h"))
    add_files(path.join(thirdPartyDir, "DDSTextureLoader/**.cpp"))
    add_includedirs(path.join(thirdPartyDir, "DDSTextureLoader"), {public = true})

    add_headerfiles(path.join(thirdPartyDir, "entt/single_include/**.hpp"))
    add_includedirs(path.join(thirdPartyDir, "entt/single_include"), {public = true})

    add_headerfiles(path.join(thirdPartyDir, "json/include/nlohmann/**.hpp"))
    add_includedirs(path.join(thirdPartyDir, "json/include"), {public = true})

    add_headerfiles(path.join(thirdPartyDir, "ImGuizmo/*.h"))
    add_files(path.join(thirdPartyDir, "ImGuizmo/*.cpp"))
    add_includedirs(path.join(thirdPartyDir, "ImGuizmo"), {public = true})

target_end()
