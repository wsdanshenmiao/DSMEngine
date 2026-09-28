add_executable(VirtualFileSystemTests VirtualFileSystem/VirtualFileSystemTests.cpp)
target_compile_features(VirtualFileSystemTests PRIVATE cxx_std_23)
target_compile_options(VirtualFileSystemTests PRIVATE /utf-8)
target_include_directories(VirtualFileSystemTests PRIVATE
    "${CMAKE_SOURCE_DIR}/Engine/Source"
    "${CMAKE_SOURCE_DIR}/Engine")
target_link_libraries(VirtualFileSystemTests PRIVATE DSMEngine)
set_target_properties(VirtualFileSystemTests PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/verification/$<CONFIG>")
