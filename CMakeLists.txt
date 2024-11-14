cmake_minimum_required(VERSION 3.29)
project(volcano C)

set(CMAKE_C_STANDARD 11)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/third-party/vesuvius-c/cmake")

add_executable(volcano volcano.c vesuvius.c)

add_compile_options(-Wall -Wextra -Wpedantic -g3 -ggdb -fpic -fpie -fPIE)

if (CMAKE_BUILD_TYPE STREQUAL "Debug")
  add_compile_options(-O0)
  add_compile_definitions(DEBUG)
else ()
  add_compile_options(-O2)
  add_compile_definitions(NDEBUG)
endif ()

include_directories(third-party/json.h third-party/vesuvius-c third-party)

find_package(Blosc2 REQUIRED)
find_package(CURL REQUIRED)

add_compile_options(-g3 -Wall -Wextra)
if(UNIX)
  add_link_options(-rdynamic)
endif()

# For some reason, when using _just_ link_libraries, no libraries are actually linked because ???
# so just target_link_libraries for all executables
target_link_libraries(volcano PUBLIC -lm -rdynamic)

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


