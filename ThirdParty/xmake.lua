-- 使用仓库内 Assimp 子模块；源码由 Assimp 自带的 CMakeLists.txt 构建。
package("assimp")
    add_deps("cmake")
    set_sourcedir(path.join(os.projectdir(), "ThirdParty", "assimp"))

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

add_requires("assimp")
thirdPartyDir = path.join(os.projectdir(), "ThirdParty")

target("ThirdParty")
    set_kind("static")
    add_headerfiles("spdlog/include/**.h")
    add_includedirs(path.join(thirdPartyDir, "spdlog/include"), {public = true})


    add_headerfiles("glfw/include/GLFW/*.h")
    add_files("glfw/src/*.c")
    add_includedirs(path.join(thirdPartyDir, "glfw/include"), {public = true})

    if is_plat("windows") then
        add_defines("_GLFW_WIN32")
        add_syslinks("gdi32", "shell32")
    end


    add_headerfiles("imgui/*.h", 
        "imgui/backends/imgui_impl_dx12.h", 
        "imgui/backends/imgui_impl_glfw.h",
        "imgui/backends/imgui_impl_win32.h")
    add_files("imgui/*.cpp", 
        "imgui/backends/imgui_impl_dx12.cpp",
        "imgui/backends/imgui_impl_glfw.cpp",
        "imgui/backends/imgui_impl_win32.cpp")
    add_includedirs(path.join(thirdPartyDir, "imgui"), {public = true})


    add_linkdirs("dxc/lib")
    add_includedirs(path.join(thirdPartyDir, "dxc/include"), {public = true})


    add_headerfiles("stb_image/**.h")
    add_includedirs(path.join(thirdPartyDir, "stb_image/"), {public = true})

    
    add_headerfiles("DDSTextureLoader/**.h")
    add_files("DDSTextureLoader/**.cpp")
    add_includedirs(path.join(thirdPartyDir, "DDSTextureLoader/"), {public = true})


    add_headerfiles("entt/single_include/**.hpp")
    add_includedirs(path.join(thirdPartyDir, "entt/single_include"), {public = true})

    add_headerfiles("json/include/nlohmann/**.hpp")
    add_includedirs(path.join(thirdPartyDir, "json/include"), {public = true})

    add_headerfiles("ImGuizmo/*.h")
    add_files("ImGuizmo/*.cpp")
    add_includedirs(path.join(thirdPartyDir, "ImGuizmo/"), {public = true})
target_end()