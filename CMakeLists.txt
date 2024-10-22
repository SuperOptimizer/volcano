cmake_minimum_required(VERSION 3.29)
project(volcano C)

set(CMAKE_C_STANDARD 11)

include_directories(. third-party/)

add_executable(volcano
    volcano.c
    volcano.h
    third-party/minitiff.h
    third-party/snic.h
    third-party/mininrrd.h
    #third-party/miniz.c
    #third-party/miniz.h
    )

target_compile_options(volcano PUBLIC -Wall -Wextra -Wpedantic -g3)
target_link_libraries(volcano PUBLIC -lz)