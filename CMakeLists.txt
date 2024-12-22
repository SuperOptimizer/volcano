cmake_minimum_required(VERSION 3.29)
project(volcano C)

set(CMAKE_C_STANDARD 23)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/third-party/villa/vesuvius-c/cmake")

add_executable(volcano volcano.c)

add_compile_options(-Wpedantic -g3 -ggdb -Wall -Wextra -Weverything )

if (CMAKE_BUILD_TYPE STREQUAL "Debug")
  target_compile_options(volcano PUBLIC -Og  -fno-omit-frame-pointer)
  target_link_options(volcano PUBLIC  -fno-omit-frame-pointer)
  target_compile_definitions(volcano PUBLIC DEBUG)
else ()
  target_compile_options(volcano PUBLIC -O2 -flto)
  target_link_options(volcano PUBLIC )
  target_compile_definitions(volcano PUBLIC NDEBUG)
endif ()

include_directories(third-party/villa/vesuvius-c)

find_package(Blosc2 REQUIRED)
find_package(CURL REQUIRED)
find_package(JsonC REQUIRED)

add_compile_options(-g3 )
if(UNIX)
  add_link_options(-rdynamic)
endif()

# For some reason, when using _just_ link_libraries, no libraries are actually linked because ???
# so just target_link_libraries for all executables
target_link_libraries(volcano PUBLIC -lm -rdynamic -lz)

if(Blosc2_FOUND)
  message(STATUS "Found blosc2. Building with Zarr support")

  target_link_libraries(volcano PUBLIC Blosc2::Blosc2)
  add_compile_definitions(VESUVIUS_ZARR_IMPL)
else()
  message(STATUS "Blosc2 not found - building without Zarr support")
endif()

if(CURL_FOUND)
  message(STATUS "Found curl. Building with Curl support")
  target_link_libraries(volcano PUBLIC CURL::libcurl)
  add_compile_definitions(VESUVIUS_CURL_IMPL)
else()
  message(STATUS "CURL not found - building without CURL support")
endif()


if(JSONC_FOUND)
  target_link_libraries(volcano PUBLIC JsonC::JsonC)
else()
  message(FATAL_ERROR "json-c not found, please install json-c: https://github.com/json-c/json-c")
endif()

set(CMAKE_C_CLANG_TIDY
        clang-tidy;
        -checks=*,
        -header-filter=.*
)