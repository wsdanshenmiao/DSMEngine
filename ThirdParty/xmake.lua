thirdPartyDir = path.join(os.projectdir(), "ThirdParty")

target("spdlog")
    set_kind("static")
    add_headerfiles("spdlog/include/**.h")
    add_includedirs(path.join(thirdPartyDir, "spdlog/include"), {public = true})
target_end()

target("glfw")
    set_kind("static")
    add_headerfiles("glfw/include/GLFW/*.h")
    add_files("glfw/src/*.c")
    add_includedirs(path.join(thirdPartyDir, "glfw/include"), {public = true})

    if is_plat("windows") then
        add_defines("_GLFW_WIN32")
        add_syslinks("gdi32", "shell32")
    end
target_end()

target("imgui")
    set_kind("static")
    add_deps("glfw")
    add_headerfiles("imgui/*.h", 
        "imgui/backends/imgui_impl_dx12.h", 
        "imgui/backends/imgui_impl_glfw.h")
    add_files("imgui/*.cpp", 
        "imgui/backends/imgui_impl_dx12.cpp",
        "imgui/backends/imgui_impl_glfw.cpp")
    add_includedirs(path.join(thirdPartyDir, "imgui"), {public = true})
target_end()

target("dxc")
    set_kind("static")
    add_linkdirs("dxc/lib")
    add_includedirs(path.join(thirdPartyDir, "dxc/include"), {public = true})
target_end()