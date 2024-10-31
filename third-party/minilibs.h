#pragma once

//minilibs notes:
// - when passing pointers to a _new function in order to fill out fields in the struct (e.g. mesh_new)
//   the struct will take ownership of the pointer and the pointer shall be cleaned up in the _free function.
//   The caller loses ownership of the pointer
// - index order is in Z Y X order
// - a 0 / SUCCESS return code indicates success for functions that do NOT return a pointer


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdint.h>

#ifndef MINILIBS_HEADER_ONLY
  #ifndef MINILIBS_STATIC_LIB
    #ifndef MINILIBS_DYNAMIC_LIB
      #define MINILIBS_HEADER_ONLY
    #endif
  #endif
#endif


#ifdef MINILIBS_HEADER_ONLY
  #define PUBLIC static inline __attribute__((visibility("hidden")))
  #define PRIVATE static inline __attribute__((visibility("hidden")))
#elifdef MINILIBS_STATIC_LIB
  #define PUBLIC static inline __attribute__((visibility("default")))
  #define PRIVATE static inline __attribute__((visibility("hidden")))
#elifdef MINILIBS_DYNAMIC_LIB
  #ifdef _WIN32
    #define PUBLIC __declspec(dllexport)
    #define PRIVATE static inline
  #else
    #define PUBLIC  __attribute__((visibility("default")))
    #define PRIVATE static inline __attribute__((visibility("hidden")))
  #endif
#else
  #define PUBLIC static inline
  #define PRIVATE static inline
#endif

typedef enum errcode {
  SUCCESS = 0,
  FAIL = 1,
} errcode;


typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int8_t s8;
typedef int16_t s16;
typedef int32_t s32;
typedef int64_t s64;
typedef float f32;
typedef double f64;

#include "minichamfer.h"
#include "minicurl.h"
#include "minihistogram.h"
#include "minimath.h"
#include "minimesh.h"
#include "mininrrd.h"
#include "miniobj.h"
#include "miniply.h"
#include "minippm.h"
#include "minisnic.h"
#include "minitiff.h"
#include "miniutils.h"
#include "minivcps.h"
#include "minivol.h"
#include "minizarr.h"