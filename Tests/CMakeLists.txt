add_executable(VirtualFileSystemTests VirtualFileSystem/VirtualFileSystemTests.cpp)
target_compile_features(VirtualFileSystemTests PRIVATE cxx_std_23)
target_compile_options(VirtualFileSystemTests PRIVATE /utf-8)
target_include_directories(VirtualFileSystemTests PRIVATE
    "${CMAKE_SOURCE_DIR}/Engine/Source"
    "${CMAKE_SOURCE_DIR}/Engine")
target_link_libraries(VirtualFileSystemTests PRIVATE DSMEngine)
set_target_properties(VirtualFileSystemTests PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/verification/$<CONFIG>")

add_executable(SceneBoundsTests SceneBounds/SceneBoundsTests.cpp)
target_compile_features(SceneBoundsTests PRIVATE cxx_std_23)
target_compile_options(SceneBoundsTests PRIVATE /utf-8)
target_include_directories(SceneBoundsTests PRIVATE
    "${CMAKE_SOURCE_DIR}/Engine/Source"
    "${CMAKE_SOURCE_DIR}/Engine")
target_link_libraries(SceneBoundsTests PRIVATE DSMEngine)
set_target_properties(SceneBoundsTests PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/build/verification/scene-bounds/$<CONFIG>")

add_executable(TemporalAATests TemporalAA/TemporalAATests.cpp)
dsm_stage_dxc(TemporalAATests)
target_compile_features(TemporalAATests PRIVATE cxx_std_23)
target_compile_options(TemporalAATests PRIVATE /utf-8)
target_include_directories(TemporalAATests PRIVATE
    "${CMAKE_SOURCE_DIR}/Engine/Source"
    "${CMAKE_SOURCE_DIR}/Engine")
target_link_libraries(TemporalAATests PRIVATE DSMEngine)
set_target_properties(TemporalAATests PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/build/verification/temporal-aa/$<CONFIG>")
