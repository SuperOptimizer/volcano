#pragma once

#include <ctype.h>
#include <limits.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <float.h>
#include <stdint.h>
#include <stdbool.h>
#include <assert.h>

//#include <curl/curl.h>
//#include <blosc2.h>

#include "minitiff.h"

typedef uint8_t u8;
typedef int8_t s8;
typedef uint16_t u16;
typedef int16_t s16;
typedef uint32_t u32;
typedef int32_t s32;
typedef uint64_t u64;
typedef int64_t s64;
typedef float f32;
typedef double f64;

// Vocab:
// A volume is an entire scroll
//   - for Scroll 1 it is all 14376 x 7888 x 8096 voxels
//   - the dtype is uint8 or uint16
// A chunk is a 3d cross section of data
//   - this could be a 512x512x512 section starting at 2000x2000x2000 and ending at 2512 x 2512 x 2512
//   - the dtype is float32
// A slice is a 2d cross section of data

// Notes:
//   - index order is in Z Y X order
//     - increasing Z means increasing through the slice. e.g. 1000.tif -> 1001.tif
//     - increasing Y means looking farther down in a slice
//     - increasing X means looking farther right in a slice
//   - a 0 return code indicates success for functions that do NOT return a pointer

typedef enum errcode{
  SUCCESS = 0,
  FAIL = 1,
} errcode;


typedef struct volume {
  s32 dims[3];
  bool is_zarr;
  bool is_tif_stack;
  bool uses_3d_tif;
  bool local_storage;
} volume;

typedef struct chunk {
  s32 dims[3];
  f32 data[];
} chunk  __attribute__((aligned(16)));

typedef struct slice {
  s32 dims[2];
  f32 data[];
} slice  __attribute__((aligned(16)));

static inline volume* volume_new(s32 dims[static 3], bool is_zarr, bool is_tif_stack, bool uses_3d_tif, bool local_storage) {
  assert (!(is_zarr && (is_tif_stack || uses_3d_tif)));

  volume* ret = malloc(sizeof(volume));
  if(ret == NULL) {
    assert(false);
    return NULL;
  }

  *ret = (volume){{dims[0], dims[1], dims[2]}, is_zarr, is_tif_stack, uses_3d_tif, local_storage};
  return ret;
}

static inline chunk* chunk_new(s32 dims[static 3]) {
  chunk* ret = malloc(sizeof(chunk) + dims[0] * dims[1] * dims[2] * sizeof(f32));

  if(ret == NULL) {
    assert(false);
    return NULL;
  }

  for (int i = 0; i < 3; i++) {
    ret->dims[i] = dims[i];
  }
  return ret;
}

static inline errcode chunk_fill(chunk* chunk, volume* vol, s32 start[static 3]) {
  if(start[0] + chunk->dims[0] < 0 || start[0] + chunk->dims[0] > vol->dims[0]) {
    assert(false);
    return FAIL;
  }
  if(start[1] + chunk->dims[1] < 0 || start[1] + chunk->dims[1] > vol->dims[1]) {
    assert(false);
    return FAIL;
  }
  if(start[2] + chunk->dims[2] < 0 || start[2] + chunk->dims[2] > vol->dims[2]) {
    assert(false);
    return FAIL;
  }

  for (int z = 0; z < vol->dims[0]; z++) {
    for (int y = 0; y < vol->dims[1]; y++) {
      for (int x = 0; x < vol->dims[2]; x++) {
        //TODO: actually get the data
        chunk->data[z*chunk->dims[1] * chunk->dims[2] + y*chunk->dims[2] + x] = 0.0f;
      }
    }
  }
  return SUCCESS;
}

static inline slice* slice_new(s32 dims[static 2]) {
  slice* ret = malloc(sizeof(slice) + dims[0] * dims[1] * sizeof(f32));

  if(ret == NULL) {
    assert(false);
    return NULL;
  }

  for (int i = 0; i < 2; i++) {
    ret->dims[i] = dims[i];
  }
  return ret;
}

static inline errcode slice_fill(slice* slice, volume* vol, s32 start[static 2], s32 axis) {

  assert(axis == 'z' || axis == 'y' || axis == 'x');
  if(start[0] + slice->dims[0] < 0 || start[0] + slice->dims[0] > vol->dims[0]) {
    assert(false);
    return FAIL;
  }
  if(start[1] + slice->dims[1] < 0 || start[1] + slice->dims[1] > vol->dims[1]) {
    assert(false);
    return FAIL;
  }

  for (int y = 0; y < vol->dims[0]; y++) {
    for (int x = 0; x < vol->dims[1]; x++) {
      //TODO: actually get the data
      slice->data[y*slice->dims[1] + x] = 0.0f;
    }
  }
  return SUCCESS;
}

chunk* tiff_to_chunk(const char* tiffpath) {
  TiffImage* img = readTIFF(tiffpath);
  if (!img || !img->isValid) {
    assert(false);
    return NULL;
  }
  if(img->depth <= 1) {
    printf("can't load a 2d tiff as a chunk");
    assert(false);
    return NULL;
  }

  //TODO: can we assume that all 3D tiffs have the same x,y dimensions for all slices? because we are right here
  s32 dims[3] = {img->depth, img->directories[0].height, img->directories[0].width};
  chunk* ret = chunk_new(dims);
  for (s32 z = 0; z < dims[0]; z++) {
    void* buf = readTIFFDirectoryData(img, z);
    for (s32 y = 0; y < dims[1]; y++) {
      for (s32 x = 0; x < dims[2]; x++) {
        if (img->directories[z].bitsPerSample == 8) {
          ret->data[z*dims[1]*dims[2] + y*dims[2] + x] = getTIFFPixel8FromBuffer(buf,y,x,img->directories[z].width);
        } else if (img->directories[z].bitsPerSample == 16) {
          ret->data[z*dims[1]*dims[2] + y*dims[2] + x] = getTIFFPixel16FromBuffer(buf,y,x,img->directories[z].width);
        }
      }
    }
  }

  printAllTIFFTags(img);

  return ret;
}