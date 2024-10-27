cmake_minimum_required(VERSION 3.29)
project(volcano C)

set(CMAKE_C_STANDARD 11)

add_compile_options(-Wall -Wextra -Wpedantic -g3)
#link_libraries(-lz)


if (CMAKE_BUILD_TYPE STREQUAL "Debug")
  add_compile_options(-g3)
  add_compile_definitions(DEBUG)
else ()
  add_compile_options(-O2)
  add_compile_definitions(NDEBUG)
endif ()

include_directories(. third-party/)

add_executable(volcanotest
    examples/volcano.c
    volcano.h
    third-party/json.h/json.h
    third-party/minitiff.h
    third-party/snic.h
    third-party/mininrrd.h
    third-party/miniz.h
    third-party/minihistogram.h
    third-party/minippm.h
    third-party/miniobj.h
    third-party/minimesher.h
    third-party/miniply.h
    third-party/minizarr.h
    third-party/minimath.h
)

