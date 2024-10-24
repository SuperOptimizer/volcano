cmake_minimum_required(VERSION 3.29)
project(volcano C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 23)

add_compile_options(-Wall -Wextra -Wpedantic -g3 -fpermissive)
#link_libraries(-lz)


if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_compile_options(-g3 )
    add_compile_definitions(DEBUG)
else()
    add_compile_options(-O2)
    add_compile_definitions(NDEBUG)
endif()

include_directories(. third-party/)

add_executable(volcanotest
    examples/volcano.c
    volcano.h
    third-party/minitiff.h
    third-party/snic.h
    third-party/mininrrd.h
    third-party/miniz.h
    )


find_package(VTK REQUIRED)
message(STATUS "Found VTK version: ${VTK_VERSION}")
find_package(OpenGL)

add_executable(volcanorenderer
  examples/renderer.cpp
  volcano.h)

set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)


add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/third-party/glfw)

# OpenGL Loader - GL3W
# This can be replaced with your own OpenGL Loader
# Either compile it as a static library like this example
# Or add it to the source files for the main executable + imgui + vtkviewer
set(gl3w_dir ${CMAKE_CURRENT_SOURCE_DIR}/third-party/gl3w)
set(gl3w_inc ${gl3w_dir}/include)
set(gl3w_src ${gl3w_dir}/src/gl3w.c)
if (EXISTS ${gl3w_src})
    message(STATUS "Found gl3w source files")
else()
    execute_process(COMMAND python3 gl3w_gen.py WORKING_DIRECTORY ${gl3w_dir}) # use python or python3
    if (NOT EXISTS ${gl3w_src})
        message(FATAL_ERROR "Could not generate gl3w source files")
    endif()
endif()
add_library(gl3w STATIC ${gl3w_src})
target_include_directories(gl3w PUBLIC ${gl3w_inc})
target_link_libraries(${EXEC_NAME} gl3w)


set(imgui_dir ${CMAKE_CURRENT_SOURCE_DIR}/third-party/imgui)
file(GLOB imgui_src ${imgui_dir}/*.cpp)
set(imgui_backend ${imgui_dir}/backends/imgui_impl_glfw.cpp ${imgui_dir}/backends/imgui_impl_opengl3.cpp)
add_library(imgui STATIC ${imgui_src} ${imgui_backend})
target_include_directories(imgui PUBLIC ${imgui_dir} ${imgui_dir}/backends)
target_link_libraries(imgui PUBLIC glfw)

set(imgui_vtk_viewer_dir ${CMAKE_CURRENT_SOURCE_DIR}/third-party/imgui-vtk)
add_library(imgui_vtk_viewer STATIC ${imgui_vtk_viewer_dir}/VtkViewer.cpp)
target_include_directories(imgui_vtk_viewer PUBLIC ${imgui_vtk_viewer_dir})
target_link_libraries(imgui_vtk_viewer gl3w) # Since gl3w was compiled as a static library, we need to link to it
target_link_libraries(imgui_vtk_viewer imgui) # Since imgui was compiled as a static library, we need to link to it
target_link_libraries(imgui_vtk_viewer ${VTK_LIBRARIES})


target_link_libraries(volcanorenderer PUBLIC  OpenGL::GL glfw imgui imgui_vtk_viewer ${VTK_LIBRARIES})
target_include_directories(volcanorenderer PUBLIC third-party/glfw/include third-party/imgui third-party/imgui/backends third-party/imgui-vtk)