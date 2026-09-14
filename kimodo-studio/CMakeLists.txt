cmake_minimum_required(VERSION 3.25)
project(kimodo_studio LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)

# --- Raylib: pinned FetchContent (system tree at C:/raylib/raylib is corrupt, revisit for v6) ---
option(KIMODO_STUDIO_USE_SYSTEM_RAYLIB "Use system Raylib source tree" OFF)
set(KIMODO_STUDIO_RAYLIB_SRC "C:/raylib/raylib" CACHE PATH "System Raylib source tree")
if(KIMODO_STUDIO_USE_SYSTEM_RAYLIB AND EXISTS "${KIMODO_STUDIO_RAYLIB_SRC}/CMakeLists.txt")
    message(STATUS "Kimodo Studio: using system Raylib at ${KIMODO_STUDIO_RAYLIB_SRC}")
    set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(BUILD_GAMES OFF CACHE BOOL "" FORCE)
    add_subdirectory("${KIMODO_STUDIO_RAYLIB_SRC}" "${CMAKE_BINARY_DIR}/raylib")
    set(_raylib_target raylib)
else()
    message(STATUS "Kimodo Studio: fetching pinned raylib 5.5")
    FetchContent_Declare(
        raylib
        GIT_REPOSITORY https://github.com/raysan5/raylib.git
        GIT_TAG 5.5
    )
    set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(BUILD_GAMES OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(raylib)
    set(_raylib_target raylib)
endif()

# --- Dear ImGui (docking branch: matches rlImGui main API) ---
FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG docking
)
FetchContent_MakeAvailable(imgui)
set(_imgui_src "${imgui_SOURCE_DIR}")

# --- rlImGui bridge (pinned to a commit tested with raylib 5.x) ---
FetchContent_Declare(
    rlimGui
    GIT_REPOSITORY https://github.com/raylib-extras/rlImGui.git
    GIT_TAG main
)
FetchContent_MakeAvailable(rlimGui)
set(_rlimgui_src "${rlimgui_SOURCE_DIR}")

add_library(studio_imgui STATIC
    "${_imgui_src}/imgui.cpp"
    "${_imgui_src}/imgui_draw.cpp"
    "${_imgui_src}/imgui_tables.cpp"
    "${_imgui_src}/imgui_widgets.cpp"
    "${_rlimgui_src}/rlImGui.cpp"
)
target_include_directories(studio_imgui PUBLIC "${_imgui_src}" "${_rlimgui_src}")
target_link_libraries(studio_imgui PUBLIC ${_raylib_target})
set_target_properties(studio_imgui PROPERTIES CXX_STANDARD 23)

add_executable(kimodo_studio
    src/main.cpp
    src/app/Application.cpp
    src/app/AppState.cpp
    src/ui/UIManager.cpp
    src/ui/Theme.cpp
    src/rendering/Viewport.cpp
    src/rendering/GridRenderer.cpp
    src/utils/Logger.cpp
    src/kimodo/KimodoAdapter.cpp
    src/kimodo/KimodoEngine.cpp
    src/animation/Skeleton.cpp
    src/animation/Animation.cpp
    src/animation/AnimationPlayer.cpp
)
target_include_directories(kimodo_studio PRIVATE src)
target_link_libraries(kimodo_studio PRIVATE studio_imgui ${_raylib_target})

# --- Kimodo backend: prebuilt MSVC libs (Phase 2; MSVC only) ---
# Root build system untouched: link existing Release artifacts.
set(KIMODO_ROOT "E:/kimodo.cpp" CACHE PATH "Kimodo root (headers + prebuilt libs)")
if(MSVC)
    target_include_directories(kimodo_studio PRIVATE "${KIMODO_ROOT}/include")
    target_compile_definitions(kimodo_studio PRIVATE KIMODO_HAVE_BACKEND=1)
    target_link_libraries(kimodo_studio PRIVATE
        "${KIMODO_ROOT}/build/Release/kimodo.lib"
        "${KIMODO_ROOT}/build/ggml/src/Release/ggml.lib"
        "${KIMODO_ROOT}/build/ggml/src/Release/ggml-base.lib"
        "${KIMODO_ROOT}/build/ggml/src/Release/ggml-cpu.lib"
    )
    if(EXISTS "${KIMODO_ROOT}/build/ggml/src/ggml-vulkan/Release/ggml-vulkan.lib")
        target_link_libraries(kimodo_studio PRIVATE
            "${KIMODO_ROOT}/build/ggml/src/ggml-vulkan/Release/ggml-vulkan.lib")
    endif()
    # ggml shared DLLs must sit next to the exe at runtime.
    add_custom_command(TARGET kimodo_studio POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${KIMODO_ROOT}/build/bin/Release"
            "$<TARGET_FILE_DIR:kimodo_studio>"
    )
endif()

if(MSVC)
    target_compile_options(kimodo_studio PRIVATE /W4 /permissive-)
    target_compile_definitions(kimodo_studio PRIVATE _CRT_SECURE_NO_WARNINGS)
    target_compile_options(studio_imgui PRIVATE /W0)
else()
    target_compile_options(kimodo_studio PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(studio_imgui PRIVATE -w)
endif()
