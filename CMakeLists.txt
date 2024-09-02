cmake_minimum_required(VERSION 3.22)

project("system-monitor" LANGUAGES CXX)

set(CMAKE_CXX_STANDARD             17)
set(CMAKE_CXX_STANDARD_REQUIRED   YES)

find_package(OpenGL REQUIRED)
find_package(Freetype REQUIRED)

set(SYSTEM_MONITOR_SOURCES
    ${CMAKE_SOURCE_DIR}/src/main.cpp
    ${CMAKE_SOURCE_DIR}/src/draw_app.cpp
    ${CMAKE_SOURCE_DIR}/src/refresh_data.cpp
    ${CMAKE_SOURCE_DIR}/src/utilities.cpp
)

set(IMGUI_SOURCES
    ${CMAKE_SOURCE_DIR}/src/imgui/imgui.cpp
    ${CMAKE_SOURCE_DIR}/src/imgui/imgui_tables.cpp
    ${CMAKE_SOURCE_DIR}/src/imgui/imgui_draw.cpp
    ${CMAKE_SOURCE_DIR}/src/imgui/imgui_widgets.cpp
    ${CMAKE_SOURCE_DIR}/src/imgui/imgui_freetype.cpp
    ${CMAKE_SOURCE_DIR}/src/imgui/backends/imgui_impl_glfw.cpp
    ${CMAKE_SOURCE_DIR}/src/imgui/backends/imgui_impl_opengl3.cpp
)

add_subdirectory(${CMAKE_SOURCE_DIR}/src/glfw)
add_executable(
    "system-monitor"
    ${SYSTEM_MONITOR_SOURCES}
    ${IMGUI_SOURCES}
)

include_directories("system-monitor" PRIVATE ${CMAKE_SOURCE_DIR}/src/glfw/include)
include_directories("system-monitor" PRIVATE ${FREETYPE_INCLUDE_DIRS})

target_include_directories("system-monitor" PRIVATE ${PROJECT_SOURCE_DIR}/src)
target_include_directories("system-monitor" PRIVATE ${PROJECT_SOURCE_DIR}/src/imgui)
target_include_directories("system-monitor" PRIVATE ${PROJECT_SOURCE_DIR}/src/imgui/backends)

target_link_libraries("system-monitor" glfw ${GLFW_LIBRARIES})
target_link_libraries("system-monitor" ${OPENGL_LIBRARIES})
target_link_libraries("system-monitor" ${FREETYPE_LIBRARIES})
