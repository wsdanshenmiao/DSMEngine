add_subdirectory(ThirdParty)

file(GLOB_RECURSE DSM_RUNTIME_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/Runtime/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/Runtime/*.h")
add_library(DSMEngine STATIC ${DSM_RUNTIME_SOURCES})
target_compile_features(DSMEngine PUBLIC cxx_std_23)
target_compile_options(DSMEngine PRIVATE /utf-8)
target_include_directories(DSMEngine PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/Source"
    "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(DSMEngine PUBLIC EngineThirdParty)
target_link_libraries(DSMEngine PRIVATE
    EngineAssimp EngineDXC EngineDDSTextureLoader
    d3d12 dxgi d3dcompiler dxguid user32 comdlg32)
set_target_properties(DSMEngine PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/engine/$<CONFIG>")

if(DSM_BUILD_EDITOR)
    file(GLOB_RECURSE DSM_EDITOR_SOURCES CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/Source/Editor/*.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/Source/Editor/*.h")
    add_library(DSMEditor STATIC ${DSM_EDITOR_SOURCES})
    target_compile_options(DSMEditor PRIVATE /utf-8)
    target_include_directories(DSMEditor PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/Source"
        "${CMAKE_CURRENT_SOURCE_DIR}")
    target_link_libraries(DSMEditor PUBLIC DSMEngine EngineImGui EngineImGuizmo)
    set_target_properties(DSMEditor PROPERTIES
        ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/engine/$<CONFIG>")
endif()
