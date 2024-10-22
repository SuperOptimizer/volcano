cmake_minimum_required(VERSION 3.29)
project(volcano C)

set(CMAKE_C_STANDARD 11)

include_directories(. third-party/)

add_executable(volcano
    volcano.c
    volcano.h
    third-party/minitiff.h)

target_compile_options(volcano PUBLIC -Wall -Wextra -Wpedantic -g3)