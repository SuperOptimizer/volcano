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

set(SOURCES volcano.h
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
    third-party/minicurl.h)

add_executable(volcanotest
    examples/runtests.c
    ${SOURCES}
)

add_executable(volcano examples/volcano.c ${SOURCES})

set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build shared libraries" FORCE)
set(BUILD_STATIC_LIBS ON CACHE BOOL "Build static libraries" FORCE)
set(CURL_STATICLIB ON CACHE BOOL "Build libcurl with static linking" FORCE)
set(CURL_DISABLE_LDAP ON CACHE BOOL "Disable LDAP" FORCE)
set(CURL_STATIC_CRT ON CACHE BOOL "Link the static CRT" FORCE)
set(BUILD_CURL_EXE OFF CACHE BOOL "Build curl executable" FORCE)

# TODO: figure out what we actually need and dont
set(CURL_DISABLE_DICT ON CACHE BOOL "Disable DICT" FORCE)
set(CURL_DISABLE_FILE ON CACHE BOOL "Disable FILE" FORCE)
set(CURL_DISABLE_FTP ON CACHE BOOL "Disable FTP" FORCE)
set(CURL_DISABLE_GOPHER ON CACHE BOOL "Disable GOPHER" FORCE)
set(CURL_DISABLE_MQTT ON CACHE BOOL "Disable MQTT" FORCE)
set(CURL_DISABLE_POP3 ON CACHE BOOL "Disable POP3" FORCE)
set(CURL_DISABLE_RTSP ON CACHE BOOL "Disable RTSP" FORCE)
set(CURL_DISABLE_SMB ON CACHE BOOL "Disable SMB" FORCE)
set(CURL_DISABLE_SMTP ON CACHE BOOL "Disable SMTP" FORCE)
set(CURL_DISABLE_TELNET ON CACHE BOOL "Disable TELNET" FORCE)
set(CURL_DISABLE_TFTP ON CACHE BOOL "Disable TFTP" FORCE)

add_subdirectory(third-party/curl)

target_link_libraries(volcanotest PUBLIC libcurl_static)
target_link_libraries(volcano PUBLIC libcurl_static)

target_include_directories(volcanotest PUBLIC third-party/curl/include)
target_include_directories(volcano PUBLIC third-party/curl/include)

if(WIN32)
  target_compile_definitions(volcanotest PUBLIC CURL_STATICLIB)
  target_compile_definitions(volcano PUBLIC CURL_STATICLIB)
  target_link_libraries(volcanotest PUBLIC ws2_32 crypt32 normaliz wldap32)
  target_link_libraries(volcano PUBLIC ws2_32 crypt32 normaliz wldap32)
endif()