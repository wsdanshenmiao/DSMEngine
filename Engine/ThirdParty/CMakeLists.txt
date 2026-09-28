set(DSM_THIRD_PARTY_DIR "${CMAKE_CURRENT_SOURCE_DIR}")

foreach(required IN ITEMS assimp glfw dxc entt imgui ImGuizmo json spdlog stb_image DDSTextureLoader)
    if(NOT EXISTS "${DSM_THIRD_PARTY_DIR}/${required}")
        message(FATAL_ERROR "缺少本地第三方依赖: ${DSM_THIRD_PARTY_DIR}/${required}")
    endif()
endforeach()

set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_ASSIMP_TOOLS OFF CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_SAMPLES OFF CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_FRAMEWORK OFF CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_ZLIB ON CACHE BOOL "" FORCE)
set(ASSIMP_NO_EXPORT ON CACHE BOOL "" FORCE)
set(ASSIMP_INSTALL OFF CACHE BOOL "" FORCE)
set(ASSIMP_WARNINGS_AS_ERRORS OFF CACHE BOOL "" FORCE)
set(ASSIMP_INJECT_DEBUG_POSTFIX OFF CACHE BOOL "" FORCE)
add_subdirectory(assimp EXCLUDE_FROM_ALL)
add_library(EngineAssimp INTERFACE)
target_link_libraries(EngineAssimp INTERFACE assimp::assimp)

set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
add_subdirectory(glfw EXCLUDE_FROM_ALL)
add_library(EngineGLFW INTERFACE)
target_link_libraries(EngineGLFW INTERFACE glfw)

add_library(EngineEnTT INTERFACE)
target_include_directories(EngineEnTT INTERFACE "${DSM_THIRD_PARTY_DIR}/entt/single_include")
add_library(EngineJson INTERFACE)
target_include_directories(EngineJson INTERFACE "${DSM_THIRD_PARTY_DIR}/json/include")
add_library(EngineSpdlog INTERFACE)
target_include_directories(EngineSpdlog INTERFACE "${DSM_THIRD_PARTY_DIR}/spdlog/include")
add_library(EngineStbImage INTERFACE)
target_include_directories(EngineStbImage INTERFACE "${DSM_THIRD_PARTY_DIR}/stb_image")

add_library(EngineDDSTextureLoader STATIC DDSTextureLoader/DDSTextureLoader12.cpp)
target_include_directories(EngineDDSTextureLoader PUBLIC "${DSM_THIRD_PARTY_DIR}/DDSTextureLoader")

add_library(EngineImGui STATIC
    imgui/imgui.cpp imgui/imgui_draw.cpp imgui/imgui_tables.cpp imgui/imgui_widgets.cpp
    imgui/backends/imgui_impl_dx12.cpp imgui/backends/imgui_impl_glfw.cpp
    imgui/backends/imgui_impl_win32.cpp)
target_include_directories(EngineImGui PUBLIC "${DSM_THIRD_PARTY_DIR}/imgui")
target_link_libraries(EngineImGui PUBLIC EngineGLFW d3d12)

add_library(EngineImGuizmo STATIC
    ImGuizmo/ImGuizmo.cpp ImGuizmo/ImCurveEdit.cpp
    ImGuizmo/ImGradient.cpp ImGuizmo/ImSequencer.cpp ImGuizmo/GraphEditor.cpp)
target_include_directories(EngineImGuizmo PUBLIC "${DSM_THIRD_PARTY_DIR}/ImGuizmo")
target_link_libraries(EngineImGuizmo PUBLIC EngineImGui)

add_library(EngineDXC INTERFACE)
target_include_directories(EngineDXC INTERFACE "${DSM_THIRD_PARTY_DIR}/dxc/include")
# 当前子模块只有 DXC 源码/头文件；导入库由 Windows SDK 提供，绝不查找用户包缓存。
target_link_libraries(EngineDXC INTERFACE dxcompiler)

add_library(EngineThirdParty INTERFACE)
target_link_libraries(EngineThirdParty INTERFACE
    EngineGLFW EngineEnTT EngineJson EngineSpdlog EngineStbImage EngineImGui EngineImGuizmo)
