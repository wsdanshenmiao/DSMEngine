thirdPartyDir = path.join(os.projectdir(), "ThirdParty")

target("spdlog")
    set_kind("static")
    add_headerfiles("spdlog/include/**.h")
    add_includedirs(path.join(thirdPartyDir, "spdlog/include"), {public = true})
target_end()