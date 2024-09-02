cmake_minimum_required(VERSION 3.22)

project(
    system-monitor
    VERSION 0.1
)

set(CMAKE_C_STANDARD               17)
set(CMAKE_CXX_STANDARD             17)
set(CMAKE_CXX_STANDARD_REQUIRED   YES)

find_package(OpenGL REQUIRED)
find_package(SDL2 REQUIRED)
find_package(Freetype REQUIRED)

add_definitions(-DIMGUI_IMPL_OPENGL_LOADER_GL3W)

set(IMGUI_SOURCES
    ${CMAKE_SOURCE_DIR}/src/imgui/imgui.cpp
    ${CMAKE_SOURCE_DIR}/src/imgui/imgui_tables.cpp
    ${CMAKE_SOURCE_DIR}/src/imgui/imgui_draw.cpp
    ${CMAKE_SOURCE_DIR}/src/imgui/imgui_widgets.cpp
    ${CMAKE_SOURCE_DIR}/src/imgui/imgui_freetype.cpp
    ${CMAKE_SOURCE_DIR}/src/imgui/backends/imgui_impl_sdl.cpp
    ${CMAKE_SOURCE_DIR}/src/imgui/backends/imgui_impl_opengl3.cpp   
)

set(GL3W_SOURCES
    ${CMAKE_SOURCE_DIR}/src/gl3w/GL/gl3w.c 
)

add_executable(
    system-monitor
    ${CMAKE_SOURCE_DIR}/src/main.cpp
    ${CMAKE_SOURCE_DIR}/src/system.cpp
    ${CMAKE_SOURCE_DIR}/src/storage.cpp
    ${CMAKE_SOURCE_DIR}/src/humanize.cpp
    ${CMAKE_SOURCE_DIR}/src/network.cpp
    ${CMAKE_SOURCE_DIR}/src/main.cpp
    ${CMAKE_SOURCE_DIR}/src/processes.cpp
    ${IMGUI_SOURCES}
    ${GL3W_SOURCES}
)

target_include_directories(system-monitor PRIVATE ${PROJECT_SOURCE_DIR}/src)
target_include_directories(system-monitor PRIVATE ${PROJECT_SOURCE_DIR}/src/imgui)
target_include_directories(system-monitor PRIVATE ${PROJECT_SOURCE_DIR}/src/imgui/backends)
target_include_directories(system-monitor PRIVATE ${PROJECT_SOURCE_DIR}/src/gl3w)
target_include_directories(system-monitor PRIVATE ${FREETYPE_INCLUDE_DIRS})

target_link_libraries(system-monitor ${OPENGL_LIBRARIES})
target_link_libraries(system-monitor ${SDL2_LIBRARIES})
target_link_libraries(system-monitor ${FREETYPE_LIBRARIES})
