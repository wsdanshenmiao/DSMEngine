thirdPartyDir = path.join(os.projectdir(), "ThirdParty")

target("spdlog")
    set_kind("static")
    add_headerfiles("spdlog/include/**.h")
    add_includedirs(path.join(thirdPartyDir, "spdlog/include"), {public = true})
target_end()

target("imgui")
    set_kind("static")
    add_headerfiles("imgui/*.h", 
        "imgui/backends/imgui_impl_dx12.h", 
        "imgui/backends/imgui_impl_win32.h")
    add_files("imgui/*.cpp", 
        "imgui/backends/imgui_impl_dx12.cpp",
        "imgui/backends/imgui_impl_win32.cpp")
    add_includedirs(path.join(thirdPartyDir, "imgui"), {public = true})
target_end()