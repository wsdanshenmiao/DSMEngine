file(GLOB_RECURSE DSM_RAYTRACING_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/RayTracing/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/RayTracing/*.h")
add_executable(RayTracing ${DSM_RAYTRACING_SOURCES})
target_link_libraries(RayTracing PRIVATE DSMEditor)
dsm_stage_dxc(RayTracing)
target_compile_options(RayTracing PRIVATE /utf-8)
set_target_properties(RayTracing PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/Binaries/$<CONFIG>")
