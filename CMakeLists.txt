cmake_minimum_required(VERSION 3.25)
project(borderlesser)

set(PROJECT_NAME borderlesser)

set(CMAKE_CXX_STANDARD 17)

find_package(OpenGL REQUIRED)

if (WIN32)
    add_executable(${PROJECT_NAME}
            src/borderlesser.cpp
            src/imgui/imgui_demo.cpp
            src/imgui/imgui_tables.cpp
            src/imgui/imgui.cpp
            src/imgui/imgui_draw.cpp
            src/imgui/imgui_internal.h
            src/imgui/imgui_widgets.cpp
            src/imgui/imgui_impl_glfw.cpp
            src/imgui/imgui_impl_opengl3.cpp
            )
endif (WIN32)

target_include_directories(${PROJECT_NAME} PRIVATE src ${OPENGL_INCLUDE_DIRS})

add_subdirectory(external/glfw)

target_link_libraries(${PROJECT_NAME} glfw ${OPENGL_LIBRARIES})