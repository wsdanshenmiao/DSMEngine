file(GLOB_RECURSE DSM_PBR_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/PBR/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/PBR/*.h")
add_executable(PBR ${DSM_PBR_SOURCES})
target_link_libraries(PBR PRIVATE DSMEditor)
dsm_stage_dxc(PBR)
target_compile_options(PBR PRIVATE /utf-8)
set_target_properties(PBR PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/Binaries/$<CONFIG>")
