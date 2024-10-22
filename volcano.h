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
#include <math.h>

//#include <curl/curl.h>
//#include <blosc2.h>

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


#include "minitiff.h"
#include "mininrrd.h"
#include "snic.h"


static inline f32 maxf32(f32 a, f32 b) { return a > b ? a : b; }
static inline f32 minf32(f32 a, f32 b) { return a < b ? a : b; }
static inline f32 avgf32(f32* data, s32 len) {
  f64 sum = 0.0;
  for(int i = 0; i < len; i++) sum+= data[i];
  return sum / len;
}



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
//   - a 0 / SUCCESS return code indicates success for functions that do NOT return a pointer

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

typedef struct {
  f32 x, y, z;
} vertex;

typedef struct {
  vertex* vertices;
  int* indices;
  int vertex_count;
  int index_count;
} mesh;

typedef struct histogram {
  int num_bins;
  f32 min_value;
  f32 max_value;
  f32 bin_width;
  u32* bins;
} histogram;

typedef struct hist_stats {
  f32 mean;
  f32 median;
  f32 mode;
  u32 mode_count;
  f32 std_dev;
} hist_stats;

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

static inline void chunk_free(chunk* chunk) {}

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

static inline chunk* tiff_to_chunk(const char* tiffpath) {
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
  return ret;
}


static inline slice* tiff_to_slice(const char* tiffpath, int index) {
  TiffImage* img = readTIFF(tiffpath);
  if (!img || !img->isValid) {
    assert(false);
    return NULL;
  }
  if(index < 0 || index >= img->depth) {
    assert(false);
    return NULL;
  }

  s32 dims[2] = {img->directories[0].height, img->directories[0].width};
  slice* ret = slice_new(dims);

  void* buf = readTIFFDirectoryData(img, index);
  for (s32 y = 0; y < dims[0]; y++) {
    for (s32 x = 0; x < dims[1]; x++) {
      if (img->directories[index].bitsPerSample == 8) {
        ret->data[y * dims[1] +  x] = getTIFFPixel8FromBuffer(buf,y,x,img->directories[index].width);
      } else if (img->directories[index].bitsPerSample == 16) {
        ret->data[y * dims[1] +  x] = getTIFFPixel16FromBuffer(buf,y,x,img->directories[index].width);
      }
    }
  }
  return ret;
}

static f32 slice_at(slice* slice, s32 y, s32 x) {
  return slice->data[y * slice->dims[1] + x];
}

static void slice_set(slice* slice, s32 y, s32 x, f32 data) {
  slice->data[y * slice->dims[1] + x] = data;
}

static f32 chunk_at(chunk* chunk, s32 z, s32 y, s32 x) {
  return chunk->data[z * chunk->dims[1] * chunk->dims[2] + y * chunk->dims[2] + x];
}

static void chunk_set(chunk* chunk, s32 z, s32 y, s32 x, f32 data) {
  chunk->data[z * chunk->dims[1] * chunk->dims[2] + y * chunk->dims[2] + x] = data;
}


static const int edgeTable[256]={
0x0  , 0x109, 0x203, 0x30a, 0x406, 0x50f, 0x605, 0x70c,
0x80c, 0x905, 0xa0f, 0xb06, 0xc0a, 0xd03, 0xe09, 0xf00,
0x190, 0x99 , 0x393, 0x29a, 0x596, 0x49f, 0x795, 0x69c,
0x99c, 0x895, 0xb9f, 0xa96, 0xd9a, 0xc93, 0xf99, 0xe90,
0x230, 0x339, 0x33 , 0x13a, 0x636, 0x73f, 0x435, 0x53c,
0xa3c, 0xb35, 0x83f, 0x936, 0xe3a, 0xf33, 0xc39, 0xd30,
0x3a0, 0x2a9, 0x1a3, 0xaa , 0x7a6, 0x6af, 0x5a5, 0x4ac,
0xbac, 0xaa5, 0x9af, 0x8a6, 0xfaa, 0xea3, 0xda9, 0xca0,
0x460, 0x569, 0x663, 0x76a, 0x66 , 0x16f, 0x265, 0x36c,
0xc6c, 0xd65, 0xe6f, 0xf66, 0x86a, 0x963, 0xa69, 0xb60,
0x5f0, 0x4f9, 0x7f3, 0x6fa, 0x1f6, 0xff , 0x3f5, 0x2fc,
0xdfc, 0xcf5, 0xfff, 0xef6, 0x9fa, 0x8f3, 0xbf9, 0xaf0,
0x650, 0x759, 0x453, 0x55a, 0x256, 0x35f, 0x55 , 0x15c,
0xe5c, 0xf55, 0xc5f, 0xd56, 0xa5a, 0xb53, 0x859, 0x950,
0x7c0, 0x6c9, 0x5c3, 0x4ca, 0x3c6, 0x2cf, 0x1c5, 0xcc ,
0xfcc, 0xec5, 0xdcf, 0xcc6, 0xbca, 0xac3, 0x9c9, 0x8c0,
0x8c0, 0x9c9, 0xac3, 0xbca, 0xcc6, 0xdcf, 0xec5, 0xfcc,
0xcc , 0x1c5, 0x2cf, 0x3c6, 0x4ca, 0x5c3, 0x6c9, 0x7c0,
0x950, 0x859, 0xb53, 0xa5a, 0xd56, 0xc5f, 0xf55, 0xe5c,
0x15c, 0x55 , 0x35f, 0x256, 0x55a, 0x453, 0x759, 0x650,
0xaf0, 0xbf9, 0x8f3, 0x9fa, 0xef6, 0xfff, 0xcf5, 0xdfc,
0x2fc, 0x3f5, 0xff , 0x1f6, 0x6fa, 0x7f3, 0x4f9, 0x5f0,
0xb60, 0xa69, 0x963, 0x86a, 0xf66, 0xe6f, 0xd65, 0xc6c,
0x36c, 0x265, 0x16f, 0x66 , 0x76a, 0x663, 0x569, 0x460,
0xca0, 0xda9, 0xea3, 0xfaa, 0x8a6, 0x9af, 0xaa5, 0xbac,
0x4ac, 0x5a5, 0x6af, 0x7a6, 0xaa , 0x1a3, 0x2a9, 0x3a0,
0xd30, 0xc39, 0xf33, 0xe3a, 0x936, 0x83f, 0xb35, 0xa3c,
0x53c, 0x435, 0x73f, 0x636, 0x13a, 0x33 , 0x339, 0x230,
0xe90, 0xf99, 0xc93, 0xd9a, 0xa96, 0xb9f, 0x895, 0x99c,
0x69c, 0x795, 0x49f, 0x596, 0x29a, 0x393, 0x99 , 0x190,
0xf00, 0xe09, 0xd03, 0xc0a, 0xb06, 0xa0f, 0x905, 0x80c,
0x70c, 0x605, 0x50f, 0x406, 0x30a, 0x203, 0x109, 0x0   };

static const int triTable[256][16] =
{{-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
{0, 8, 3, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
{0, 1, 9, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
{1, 8, 3, 9, 8, 1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
{1, 2, 10, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
{0, 8, 3, 1, 2, 10, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
{9, 2, 10, 0, 2, 9, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
{2, 8, 3, 2, 10, 8, 10, 9, 8, -1, -1, -1, -1, -1, -1, -1},
{3, 11, 2, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
{0, 11, 2, 8, 11, 0, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
{1, 9, 0, 2, 3, 11, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
{1, 11, 2, 1, 9, 11, 9, 8, 11, -1, -1, -1, -1, -1, -1, -1},
{3, 10, 1, 11, 10, 3, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
{0, 10, 1, 0, 8, 10, 8, 11, 10, -1, -1, -1, -1, -1, -1, -1},
{3, 9, 0, 3, 11, 9, 11, 10, 9, -1, -1, -1, -1, -1, -1, -1},
{9, 8, 10, 10, 8, 11, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
{4, 7, 8, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
{4, 3, 0, 7, 3, 4, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
{0, 1, 9, 8, 4, 7, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
{4, 1, 9, 4, 7, 1, 7, 3, 1, -1, -1, -1, -1, -1, -1, -1},
{1, 2, 10, 8, 4, 7, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
{3, 4, 7, 3, 0, 4, 1, 2, 10, -1, -1, -1, -1, -1, -1, -1},
{9, 2, 10, 9, 0, 2, 8, 4, 7, -1, -1, -1, -1, -1, -1, -1},
{2, 10, 9, 2, 9, 7, 2, 7, 3, 7, 9, 4, -1, -1, -1, -1},
{8, 4, 7, 3, 11, 2, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
{11, 4, 7, 11, 2, 4, 2, 0, 4, -1, -1, -1, -1, -1, -1, -1},
{9, 0, 1, 8, 4, 7, 2, 3, 11, -1, -1, -1, -1, -1, -1, -1},
{4, 7, 11, 9, 4, 11, 9, 11, 2, 9, 2, 1, -1, -1, -1, -1},
{3, 10, 1, 3, 11, 10, 7, 8, 4, -1, -1, -1, -1, -1, -1, -1},
{1, 11, 10, 1, 4, 11, 1, 0, 4, 7, 11, 4, -1, -1, -1, -1},
{4, 7, 8, 9, 0, 11, 9, 11, 10, 11, 0, 3, -1, -1, -1, -1},
{4, 7, 11, 4, 11, 9, 9, 11, 10, -1, -1, -1, -1, -1, -1, -1},
{9, 5, 4, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
{9, 5, 4, 0, 8, 3, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
{0, 5, 4, 1, 5, 0, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
{8, 5, 4, 8, 3, 5, 3, 1, 5, -1, -1, -1, -1, -1, -1, -1},
{1, 2, 10, 9, 5, 4, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
{3, 0, 8, 1, 2, 10, 4, 9, 5, -1, -1, -1, -1, -1, -1, -1},
{5, 2, 10, 5, 4, 2, 4, 0, 2, -1, -1, -1, -1, -1, -1, -1},
{2, 10, 5, 3, 2, 5, 3, 5, 4, 3, 4, 8, -1, -1, -1, -1},
{9, 5, 4, 2, 3, 11, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
{0, 11, 2, 0, 8, 11, 4, 9, 5, -1, -1, -1, -1, -1, -1, -1},
{0, 5, 4, 0, 1, 5, 2, 3, 11, -1, -1, -1, -1, -1, -1, -1},
{2, 1, 5, 2, 5, 8, 2, 8, 11, 4, 8, 5, -1, -1, -1, -1},
{10, 3, 11, 10, 1, 3, 9, 5, 4, -1, -1, -1, -1, -1, -1, -1},
{4, 9, 5, 0, 8, 1, 8, 10, 1, 8, 11, 10, -1, -1, -1, -1},
{5, 4, 0, 5, 0, 11, 5, 11, 10, 11, 0, 3, -1, -1, -1, -1},
{5, 4, 8, 5, 8, 10, 10, 8, 11, -1, -1, -1, -1, -1, -1, -1},
{9, 7, 8, 5, 7, 9, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
{9, 3, 0, 9, 5, 3, 5, 7, 3, -1, -1, -1, -1, -1, -1, -1},
{0, 7, 8, 0, 1, 7, 1, 5, 7, -1, -1, -1, -1, -1, -1, -1},
{1, 5, 3, 3, 5, 7, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
{9, 7, 8, 9, 5, 7, 10, 1, 2, -1, -1, -1, -1, -1, -1, -1},
{10, 1, 2, 9, 5, 0, 5, 3, 0, 5, 7, 3, -1, -1, -1, -1},
{8, 0, 2, 8, 2, 5, 8, 5, 7, 10, 5, 2, -1, -1, -1, -1},
{2, 10, 5, 2, 5, 3, 3, 5, 7, -1, -1, -1, -1, -1, -1, -1},
{7, 9, 5, 7, 8, 9, 3, 11, 2, -1, -1, -1, -1, -1, -1, -1},
{9, 5, 7, 9, 7, 2, 9, 2, 0, 2, 7, 11, -1, -1, -1, -1},
{2, 3, 11, 0, 1, 8, 1, 7, 8, 1, 5, 7, -1, -1, -1, -1},
{11, 2, 1, 11, 1, 7, 7, 1, 5, -1, -1, -1, -1, -1, -1, -1},
{9, 5, 8, 8, 5, 7, 10, 1, 3, 10, 3, 11, -1, -1, -1, -1},
{5, 7, 0, 5, 0, 9, 7, 11, 0, 1, 0, 10, 11, 10, 0, -1},
{11, 10, 0, 11, 0, 3, 10, 5, 0, 8, 0, 7, 5, 7, 0, -1},
{11, 10, 5, 7, 11, 5, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
{10, 6, 5, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
{0, 8, 3, 5, 10, 6, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
{9, 0, 1, 5, 10, 6, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
{1, 8, 3, 1, 9, 8, 5, 10, 6, -1, -1, -1, -1, -1, -1, -1},
{1, 6, 5, 2, 6, 1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
{1, 6, 5, 1, 2, 6, 3, 0, 8, -1, -1, -1, -1, -1, -1, -1},
{9, 6, 5, 9, 0, 6, 0, 2, 6, -1, -1, -1, -1, -1, -1, -1},
{5, 9, 8, 5, 8, 2, 5, 2, 6, 3, 2, 8, -1, -1, -1, -1},
{2, 3, 11, 10, 6, 5, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
{11, 0, 8, 11, 2, 0, 10, 6, 5, -1, -1, -1, -1, -1, -1, -1},
{0, 1, 9, 2, 3, 11, 5, 10, 6, -1, -1, -1, -1, -1, -1, -1},
{5, 10, 6, 1, 9, 2, 9, 11, 2, 9, 8, 11, -1, -1, -1, -1},
{6, 3, 11, 6, 5, 3, 5, 1, 3, -1, -1, -1, -1, -1, -1, -1},
{0, 8, 11, 0, 11, 5, 0, 5, 1, 5, 11, 6, -1, -1, -1, -1},
{3, 11, 6, 0, 3, 6, 0, 6, 5, 0, 5, 9, -1, -1, -1, -1},
{6, 5, 9, 6, 9, 11, 11, 9, 8, -1, -1, -1, -1, -1, -1, -1},
{5, 10, 6, 4, 7, 8, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
{4, 3, 0, 4, 7, 3, 6, 5, 10, -1, -1, -1, -1, -1, -1, -1},
{1, 9, 0, 5, 10, 6, 8, 4, 7, -1, -1, -1, -1, -1, -1, -1},
{10, 6, 5, 1, 9, 7, 1, 7, 3, 7, 9, 4, -1, -1, -1, -1},
{6, 1, 2, 6, 5, 1, 4, 7, 8, -1, -1, -1, -1, -1, -1, -1},
{1, 2, 5, 5, 2, 6, 3, 0, 4, 3, 4, 7, -1, -1, -1, -1},
{8, 4, 7, 9, 0, 5, 0, 6, 5, 0, 2, 6, -1, -1, -1, -1},
{7, 3, 9, 7, 9, 4, 3, 2, 9, 5, 9, 6, 2, 6, 9, -1},
{3, 11, 2, 7, 8, 4, 10, 6, 5, -1, -1, -1, -1, -1, -1, -1},
{5, 10, 6, 4, 7, 2, 4, 2, 0, 2, 7, 11, -1, -1, -1, -1},
{0, 1, 9, 4, 7, 8, 2, 3, 11, 5, 10, 6, -1, -1, -1, -1},
{9, 2, 1, 9, 11, 2, 9, 4, 11, 7, 11, 4, 5, 10, 6, -1},
{8, 4, 7, 3, 11, 5, 3, 5, 1, 5, 11, 6, -1, -1, -1, -1},
{5, 1, 11, 5, 11, 6, 1, 0, 11, 7, 11, 4, 0, 4, 11, -1},
{0, 5, 9, 0, 6, 5, 0, 3, 6, 11, 6, 3, 8, 4, 7, -1},
{6, 5, 9, 6, 9, 11, 4, 7, 9, 7, 11, 9, -1, -1, -1, -1},
{10, 4, 9, 6, 4, 10, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
{4, 10, 6, 4, 9, 10, 0, 8, 3, -1, -1, -1, -1, -1, -1, -1},
{10, 0, 1, 10, 6, 0, 6, 4, 0, -1, -1, -1, -1, -1, -1, -1},
{8, 3, 1, 8, 1, 6, 8, 6, 4, 6, 1, 10, -1, -1, -1, -1},
{1, 4, 9, 1, 2, 4, 2, 6, 4, -1, -1, -1, -1, -1, -1, -1},
{3, 0, 8, 1, 2, 9, 2, 4, 9, 2, 6, 4, -1, -1, -1, -1},
{0, 2, 4, 4, 2, 6, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
{8, 3, 2, 8, 2, 4, 4, 2, 6, -1, -1, -1, -1, -1, -1, -1},
{10, 4, 9, 10, 6, 4, 11, 2, 3, -1, -1, -1, -1, -1, -1, -1},
{0, 8, 2, 2, 8, 11, 4, 9, 10, 4, 10, 6, -1, -1, -1, -1},
{3, 11, 2, 0, 1, 6, 0, 6, 4, 6, 1, 10, -1, -1, -1, -1},
{6, 4, 1, 6, 1, 10, 4, 8, 1, 2, 1, 11, 8, 11, 1, -1},
{9, 6, 4, 9, 3, 6, 9, 1, 3, 11, 6, 3, -1, -1, -1, -1},
{8, 11, 1, 8, 1, 0, 11, 6, 1, 9, 1, 4, 6, 4, 1, -1},
{3, 11, 6, 3, 6, 0, 0, 6, 4, -1, -1, -1, -1, -1, -1, -1},
{6, 4, 8, 11, 6, 8, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
{7, 10, 6, 7, 8, 10, 8, 9, 10, -1, -1, -1, -1, -1, -1, -1},
{0, 7, 3, 0, 10, 7, 0, 9, 10, 6, 7, 10, -1, -1, -1, -1},
{10, 6, 7, 1, 10, 7, 1, 7, 8, 1, 8, 0, -1, -1, -1, -1},
{10, 6, 7, 10, 7, 1, 1, 7, 3, -1, -1, -1, -1, -1, -1, -1},
{1, 2, 6, 1, 6, 8, 1, 8, 9, 8, 6, 7, -1, -1, -1, -1},
{2, 6, 9, 2, 9, 1, 6, 7, 9, 0, 9, 3, 7, 3, 9, -1},
{7, 8, 0, 7, 0, 6, 6, 0, 2, -1, -1, -1, -1, -1, -1, -1},
{7, 3, 2, 6, 7, 2, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
{2, 3, 11, 10, 6, 8, 10, 8, 9, 8, 6, 7, -1, -1, -1, -1},
{2, 0, 7, 2, 7, 11, 0, 9, 7, 6, 7, 10, 9, 10, 7, -1},
{1, 8, 0, 1, 7, 8, 1, 10, 7, 6, 7, 10, 2, 3, 11, -1},
{11, 2, 1, 11, 1, 7, 10, 6, 1, 6, 7, 1, -1, -1, -1, -1},
{8, 9, 6, 8, 6, 7, 9, 1, 6, 11, 6, 3, 1, 3, 6, -1},
{0, 9, 1, 11, 6, 7, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
{7, 8, 0, 7, 0, 6, 3, 11, 0, 11, 6, 0, -1, -1, -1, -1},
{7, 11, 6, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
{7, 6, 11, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
{3, 0, 8, 11, 7, 6, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
{0, 1, 9, 11, 7, 6, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
{8, 1, 9, 8, 3, 1, 11, 7, 6, -1, -1, -1, -1, -1, -1, -1},
{10, 1, 2, 6, 11, 7, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
{1, 2, 10, 3, 0, 8, 6, 11, 7, -1, -1, -1, -1, -1, -1, -1},
{2, 9, 0, 2, 10, 9, 6, 11, 7, -1, -1, -1, -1, -1, -1, -1},
{6, 11, 7, 2, 10, 3, 10, 8, 3, 10, 9, 8, -1, -1, -1, -1},
{7, 2, 3, 6, 2, 7, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
{7, 0, 8, 7, 6, 0, 6, 2, 0, -1, -1, -1, -1, -1, -1, -1},
{2, 7, 6, 2, 3, 7, 0, 1, 9, -1, -1, -1, -1, -1, -1, -1},
{1, 6, 2, 1, 8, 6, 1, 9, 8, 8, 7, 6, -1, -1, -1, -1},
{10, 7, 6, 10, 1, 7, 1, 3, 7, -1, -1, -1, -1, -1, -1, -1},
{10, 7, 6, 1, 7, 10, 1, 8, 7, 1, 0, 8, -1, -1, -1, -1},
{0, 3, 7, 0, 7, 10, 0, 10, 9, 6, 10, 7, -1, -1, -1, -1},
{7, 6, 10, 7, 10, 8, 8, 10, 9, -1, -1, -1, -1, -1, -1, -1},
{6, 8, 4, 11, 8, 6, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
{3, 6, 11, 3, 0, 6, 0, 4, 6, -1, -1, -1, -1, -1, -1, -1},
{8, 6, 11, 8, 4, 6, 9, 0, 1, -1, -1, -1, -1, -1, -1, -1},
{9, 4, 6, 9, 6, 3, 9, 3, 1, 11, 3, 6, -1, -1, -1, -1},
{6, 8, 4, 6, 11, 8, 2, 10, 1, -1, -1, -1, -1, -1, -1, -1},
{1, 2, 10, 3, 0, 11, 0, 6, 11, 0, 4, 6, -1, -1, -1, -1},
{4, 11, 8, 4, 6, 11, 0, 2, 9, 2, 10, 9, -1, -1, -1, -1},
{10, 9, 3, 10, 3, 2, 9, 4, 3, 11, 3, 6, 4, 6, 3, -1},
{8, 2, 3, 8, 4, 2, 4, 6, 2, -1, -1, -1, -1, -1, -1, -1},
{0, 4, 2, 4, 6, 2, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
{1, 9, 0, 2, 3, 4, 2, 4, 6, 4, 3, 8, -1, -1, -1, -1},
{1, 9, 4, 1, 4, 2, 2, 4, 6, -1, -1, -1, -1, -1, -1, -1},
{8, 1, 3, 8, 6, 1, 8, 4, 6, 6, 10, 1, -1, -1, -1, -1},
{10, 1, 0, 10, 0, 6, 6, 0, 4, -1, -1, -1, -1, -1, -1, -1},
{4, 6, 3, 4, 3, 8, 6, 10, 3, 0, 3, 9, 10, 9, 3, -1},
{10, 9, 4, 6, 10, 4, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
{4, 9, 5, 7, 6, 11, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
{0, 8, 3, 4, 9, 5, 11, 7, 6, -1, -1, -1, -1, -1, -1, -1},
{5, 0, 1, 5, 4, 0, 7, 6, 11, -1, -1, -1, -1, -1, -1, -1},
{11, 7, 6, 8, 3, 4, 3, 5, 4, 3, 1, 5, -1, -1, -1, -1},
{9, 5, 4, 10, 1, 2, 7, 6, 11, -1, -1, -1, -1, -1, -1, -1},
{6, 11, 7, 1, 2, 10, 0, 8, 3, 4, 9, 5, -1, -1, -1, -1},
{7, 6, 11, 5, 4, 10, 4, 2, 10, 4, 0, 2, -1, -1, -1, -1},
{3, 4, 8, 3, 5, 4, 3, 2, 5, 10, 5, 2, 11, 7, 6, -1},
{7, 2, 3, 7, 6, 2, 5, 4, 9, -1, -1, -1, -1, -1, -1, -1},
{9, 5, 4, 0, 8, 6, 0, 6, 2, 6, 8, 7, -1, -1, -1, -1},
{3, 6, 2, 3, 7, 6, 1, 5, 0, 5, 4, 0, -1, -1, -1, -1},
{6, 2, 8, 6, 8, 7, 2, 1, 8, 4, 8, 5, 1, 5, 8, -1},
{9, 5, 4, 10, 1, 6, 1, 7, 6, 1, 3, 7, -1, -1, -1, -1},
{1, 6, 10, 1, 7, 6, 1, 0, 7, 8, 7, 0, 9, 5, 4, -1},
{4, 0, 10, 4, 10, 5, 0, 3, 10, 6, 10, 7, 3, 7, 10, -1},
{7, 6, 10, 7, 10, 8, 5, 4, 10, 4, 8, 10, -1, -1, -1, -1},
{6, 9, 5, 6, 11, 9, 11, 8, 9, -1, -1, -1, -1, -1, -1, -1},
{3, 6, 11, 0, 6, 3, 0, 5, 6, 0, 9, 5, -1, -1, -1, -1},
{0, 11, 8, 0, 5, 11, 0, 1, 5, 5, 6, 11, -1, -1, -1, -1},
{6, 11, 3, 6, 3, 5, 5, 3, 1, -1, -1, -1, -1, -1, -1, -1},
{1, 2, 10, 9, 5, 11, 9, 11, 8, 11, 5, 6, -1, -1, -1, -1},
{0, 11, 3, 0, 6, 11, 0, 9, 6, 5, 6, 9, 1, 2, 10, -1},
{11, 8, 5, 11, 5, 6, 8, 0, 5, 10, 5, 2, 0, 2, 5, -1},
{6, 11, 3, 6, 3, 5, 2, 10, 3, 10, 5, 3, -1, -1, -1, -1},
{5, 8, 9, 5, 2, 8, 5, 6, 2, 3, 8, 2, -1, -1, -1, -1},
{9, 5, 6, 9, 6, 0, 0, 6, 2, -1, -1, -1, -1, -1, -1, -1},
{1, 5, 8, 1, 8, 0, 5, 6, 8, 3, 8, 2, 6, 2, 8, -1},
{1, 5, 6, 2, 1, 6, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
{1, 3, 6, 1, 6, 10, 3, 8, 6, 5, 6, 9, 8, 9, 6, -1},
{10, 1, 0, 10, 0, 6, 9, 5, 0, 5, 6, 0, -1, -1, -1, -1},
{0, 3, 8, 5, 6, 10, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
{10, 5, 6, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
{11, 5, 10, 7, 5, 11, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
{11, 5, 10, 11, 7, 5, 8, 3, 0, -1, -1, -1, -1, -1, -1, -1},
{5, 11, 7, 5, 10, 11, 1, 9, 0, -1, -1, -1, -1, -1, -1, -1},
{10, 7, 5, 10, 11, 7, 9, 8, 1, 8, 3, 1, -1, -1, -1, -1},
{11, 1, 2, 11, 7, 1, 7, 5, 1, -1, -1, -1, -1, -1, -1, -1},
{0, 8, 3, 1, 2, 7, 1, 7, 5, 7, 2, 11, -1, -1, -1, -1},
{9, 7, 5, 9, 2, 7, 9, 0, 2, 2, 11, 7, -1, -1, -1, -1},
{7, 5, 2, 7, 2, 11, 5, 9, 2, 3, 2, 8, 9, 8, 2, -1},
{2, 5, 10, 2, 3, 5, 3, 7, 5, -1, -1, -1, -1, -1, -1, -1},
{8, 2, 0, 8, 5, 2, 8, 7, 5, 10, 2, 5, -1, -1, -1, -1},
{9, 0, 1, 5, 10, 3, 5, 3, 7, 3, 10, 2, -1, -1, -1, -1},
{9, 8, 2, 9, 2, 1, 8, 7, 2, 10, 2, 5, 7, 5, 2, -1},
{1, 3, 5, 3, 7, 5, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
{0, 8, 7, 0, 7, 1, 1, 7, 5, -1, -1, -1, -1, -1, -1, -1},
{9, 0, 3, 9, 3, 5, 5, 3, 7, -1, -1, -1, -1, -1, -1, -1},
{9, 8, 7, 5, 9, 7, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
{5, 8, 4, 5, 10, 8, 10, 11, 8, -1, -1, -1, -1, -1, -1, -1},
{5, 0, 4, 5, 11, 0, 5, 10, 11, 11, 3, 0, -1, -1, -1, -1},
{0, 1, 9, 8, 4, 10, 8, 10, 11, 10, 4, 5, -1, -1, -1, -1},
{10, 11, 4, 10, 4, 5, 11, 3, 4, 9, 4, 1, 3, 1, 4, -1},
{2, 5, 1, 2, 8, 5, 2, 11, 8, 4, 5, 8, -1, -1, -1, -1},
{0, 4, 11, 0, 11, 3, 4, 5, 11, 2, 11, 1, 5, 1, 11, -1},
{0, 2, 5, 0, 5, 9, 2, 11, 5, 4, 5, 8, 11, 8, 5, -1},
{9, 4, 5, 2, 11, 3, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
{2, 5, 10, 3, 5, 2, 3, 4, 5, 3, 8, 4, -1, -1, -1, -1},
{5, 10, 2, 5, 2, 4, 4, 2, 0, -1, -1, -1, -1, -1, -1, -1},
{3, 10, 2, 3, 5, 10, 3, 8, 5, 4, 5, 8, 0, 1, 9, -1},
{5, 10, 2, 5, 2, 4, 1, 9, 2, 9, 4, 2, -1, -1, -1, -1},
{8, 4, 5, 8, 5, 3, 3, 5, 1, -1, -1, -1, -1, -1, -1, -1},
{0, 4, 5, 1, 0, 5, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
{8, 4, 5, 8, 5, 3, 9, 0, 5, 0, 3, 5, -1, -1, -1, -1},
{9, 4, 5, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
{4, 11, 7, 4, 9, 11, 9, 10, 11, -1, -1, -1, -1, -1, -1, -1},
{0, 8, 3, 4, 9, 7, 9, 11, 7, 9, 10, 11, -1, -1, -1, -1},
{1, 10, 11, 1, 11, 4, 1, 4, 0, 7, 4, 11, -1, -1, -1, -1},
{3, 1, 4, 3, 4, 8, 1, 10, 4, 7, 4, 11, 10, 11, 4, -1},
{4, 11, 7, 9, 11, 4, 9, 2, 11, 9, 1, 2, -1, -1, -1, -1},
{9, 7, 4, 9, 11, 7, 9, 1, 11, 2, 11, 1, 0, 8, 3, -1},
{11, 7, 4, 11, 4, 2, 2, 4, 0, -1, -1, -1, -1, -1, -1, -1},
{11, 7, 4, 11, 4, 2, 8, 3, 4, 3, 2, 4, -1, -1, -1, -1},
{2, 9, 10, 2, 7, 9, 2, 3, 7, 7, 4, 9, -1, -1, -1, -1},
{9, 10, 7, 9, 7, 4, 10, 2, 7, 8, 7, 0, 2, 0, 7, -1},
{3, 7, 10, 3, 10, 2, 7, 4, 10, 1, 10, 0, 4, 0, 10, -1},
{1, 10, 2, 8, 7, 4, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
{4, 9, 1, 4, 1, 7, 7, 1, 3, -1, -1, -1, -1, -1, -1, -1},
{4, 9, 1, 4, 1, 7, 0, 8, 1, 8, 7, 1, -1, -1, -1, -1},
{4, 0, 3, 7, 4, 3, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
{4, 8, 7, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
{9, 10, 8, 10, 11, 8, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
{3, 0, 9, 3, 9, 11, 11, 9, 10, -1, -1, -1, -1, -1, -1, -1},
{0, 1, 10, 0, 10, 8, 8, 10, 11, -1, -1, -1, -1, -1, -1, -1},
{3, 1, 10, 11, 3, 10, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
{1, 2, 11, 1, 11, 9, 9, 11, 8, -1, -1, -1, -1, -1, -1, -1},
{3, 0, 9, 3, 9, 11, 1, 2, 9, 2, 11, 9, -1, -1, -1, -1},
{0, 2, 11, 8, 0, 11, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
{3, 2, 11, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
{2, 3, 8, 2, 8, 10, 10, 8, 9, -1, -1, -1, -1, -1, -1, -1},
{9, 10, 2, 0, 9, 2, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
{2, 3, 8, 2, 8, 10, 0, 1, 8, 1, 10, 8, -1, -1, -1, -1},
{1, 10, 2, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
{1, 3, 8, 9, 1, 8, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
{0, 9, 1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
{0, 3, 8, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
{-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1}};


// Helper function to interpolate between two points
static vertex interpolate_vertex(f32 isovalue,
                               f32 v1, f32 v2,
                               vertex p1, vertex p2) {
    if (fabs(isovalue - v1) < 0.00001f)
        return p1;
    if (fabs(isovalue - v2) < 0.00001f)
        return p2;
    if (fabs(v1 - v2) < 0.00001f)
        return p1;

    float mu = (isovalue - v1) / (v2 - v1);
    vertex p;
    p.x = p1.x + mu * (p2.x - p1.x);
    p.y = p1.y + mu * (p2.y - p1.y);
    p.z = p1.z + mu * (p2.z - p1.z);
    return p;
}

static void process_cube(chunk* chunk,
                        int x, int y, int z,
                        f32 isovalue,
                        mesh* out_mesh) {

    f32 cube_values[8];
    cube_values[0] = chunk_at(chunk, z, y, x);
    cube_values[1] = chunk_at(chunk, z, y, x + 1);
    cube_values[2] = chunk_at(chunk, z, y + 1, x + 1);
    cube_values[3] = chunk_at(chunk, z, y + 1, x);
    cube_values[4] = chunk_at(chunk, z + 1, y, x);
    cube_values[5] = chunk_at(chunk, z + 1, y, x + 1);
    cube_values[6] = chunk_at(chunk, z + 1, y + 1, x + 1);
    cube_values[7] = chunk_at(chunk, z + 1, y + 1, x);

    int cubeindex = 0;
    for (int i = 0; i < 8; i++) {
        if (cube_values[i] < isovalue)
            cubeindex |= (1 << i);
    }

    if (edgeTable[cubeindex] == 0)
        return;

    vertex edge_verts[12];
    if (edgeTable[cubeindex] & 1)
        edge_verts[0] = interpolate_vertex(isovalue,
            cube_values[0], cube_values[1],
            (vertex){x, y, z}, (vertex){x + 1, y, z});
    if (edgeTable[cubeindex] & 2)
        edge_verts[1] = interpolate_vertex(isovalue,
            cube_values[1], cube_values[2],
            (vertex){x + 1, y, z}, (vertex){x + 1, y + 1, z});
    if (edgeTable[cubeindex] & 4)
        edge_verts[2] = interpolate_vertex(isovalue,
            cube_values[2], cube_values[3],
            (vertex){x + 1, y + 1, z}, (vertex){x, y + 1, z});
    if (edgeTable[cubeindex] & 8)
        edge_verts[3] = interpolate_vertex(isovalue,
            cube_values[3], cube_values[0],
            (vertex){x, y + 1, z}, (vertex){x, y, z});
    if (edgeTable[cubeindex] & 16)
        edge_verts[4] = interpolate_vertex(isovalue,
            cube_values[4], cube_values[5],
            (vertex){x, y, z + 1}, (vertex){x + 1, y, z + 1});
    if (edgeTable[cubeindex] & 32)
        edge_verts[5] = interpolate_vertex(isovalue,
            cube_values[5], cube_values[6],
            (vertex){x + 1, y, z + 1}, (vertex){x + 1, y + 1, z + 1});
    if (edgeTable[cubeindex] & 64)
        edge_verts[6] = interpolate_vertex(isovalue,
            cube_values[6], cube_values[7],
            (vertex){x + 1, y + 1, z + 1}, (vertex){x, y + 1, z + 1});
    if (edgeTable[cubeindex] & 128)
        edge_verts[7] = interpolate_vertex(isovalue,
            cube_values[7], cube_values[4],
            (vertex){x, y + 1, z + 1}, (vertex){x, y, z + 1});
    if (edgeTable[cubeindex] & 256)
        edge_verts[8] = interpolate_vertex(isovalue,
            cube_values[0], cube_values[4],
            (vertex){x, y, z}, (vertex){x, y, z + 1});
    if (edgeTable[cubeindex] & 512)
        edge_verts[9] = interpolate_vertex(isovalue,
            cube_values[1], cube_values[5],
            (vertex){x + 1, y, z}, (vertex){x + 1, y, z + 1});
    if (edgeTable[cubeindex] & 1024)
        edge_verts[10] = interpolate_vertex(isovalue,
            cube_values[2], cube_values[6],
            (vertex){x + 1, y + 1, z}, (vertex){x + 1, y + 1, z + 1});
    if (edgeTable[cubeindex] & 2048)
        edge_verts[11] = interpolate_vertex(isovalue,
            cube_values[3], cube_values[7],
            (vertex){x, y + 1, z}, (vertex){x, y + 1, z + 1});

    for (int i = 0; triTable[cubeindex][i] != -1; i += 3) {
        out_mesh->vertices[out_mesh->vertex_count] = edge_verts[triTable[cubeindex][i]];
        out_mesh->vertices[out_mesh->vertex_count + 1] = edge_verts[triTable[cubeindex][i + 1]];
        out_mesh->vertices[out_mesh->vertex_count + 2] = edge_verts[triTable[cubeindex][i + 2]];

        out_mesh->indices[out_mesh->index_count] = out_mesh->vertex_count;
        out_mesh->indices[out_mesh->index_count + 1] = out_mesh->vertex_count + 1;
        out_mesh->indices[out_mesh->index_count + 2] = out_mesh->vertex_count + 2;

        out_mesh->vertex_count += 3;
        out_mesh->index_count += 3;
    }
}

mesh* march(chunk* chunk, f32 isovalue) {

    int max_triangles = (chunk->dims[0] - 1) *
                       (chunk->dims[1] - 1) *
                       (chunk->dims[2] - 1) * 5;

    mesh* out_mesh = malloc(sizeof(mesh));
    out_mesh->vertices = malloc(sizeof(vertex) * max_triangles * 3);
    out_mesh->indices = malloc(sizeof(int) * max_triangles * 3);
    out_mesh->vertex_count = 0;
    out_mesh->index_count = 0;

    for (int z = 0; z < chunk->dims[0] - 1; z++) {
        for (int y = 0; y < chunk->dims[1] - 1; y++) {
            for (int x = 0; x < chunk->dims[2] - 1; x++) {
                process_cube(chunk, x, y, z, isovalue, out_mesh);
            }
        }
    }

    out_mesh->vertices = realloc(out_mesh->vertices,
                                sizeof(vertex) * out_mesh->vertex_count);
    out_mesh->indices = realloc(out_mesh->indices,
                               sizeof(int) * out_mesh->index_count);

    return out_mesh;
}

void mesh_free(mesh* mesh) {
    if (mesh) {
        free(mesh->vertices);
        free(mesh->indices);
    }
}

errcode write_mesh_to_ply(const char* filename, const mesh* mesh) {
  FILE* fp = fopen(filename, "w");
  if (!fp) {
    return FAIL;
  }

  fprintf(fp, "ply\n");
  fprintf(fp, "format ascii 1.0\n");
  fprintf(fp, "comment Created by marching cubes implementation\n");
  fprintf(fp, "element vertex %d\n", mesh->vertex_count);
  fprintf(fp, "property float x\n");
  fprintf(fp, "property float y\n");
  fprintf(fp, "property float z\n");
  fprintf(fp, "element face %d\n", mesh->index_count / 3);
  fprintf(fp, "property list uchar int vertex_indices\n");
  fprintf(fp, "end_header\n");

  for (int i = 0; i < mesh->vertex_count; i++) {
    fprintf(fp, "%.6f %.6f %.6f\n",
        mesh->vertices[i].x,
        mesh->vertices[i].y,
        mesh->vertices[i].z);
  }

  for (int i = 0; i < mesh->index_count; i += 3) {
    fprintf(fp, "3 %d %d %d\n",
        mesh->indices[i],
        mesh->indices[i + 1],
        mesh->indices[i + 2]);
  }

  fclose(fp);
  return SUCCESS;
}

/*
static chunk maxpool(chunk* inchunk, s32 kernel, s32 stride) {
  chunk ret = chunk_new(inchunk->dtype, (inchunk->depth + stride - 1) / stride, (inchunk->height + stride - 1) / stride,
                        (inchunk->width + stride - 1) / stride);
  for (s32 z = 0; z < ret.depth; z++)
    for (s32 y = 0; y < ret.height; y++)
      for (s32 x = 0; x < ret.width; x++) {
        u8 max8 = 0;
        f32 max32 = -INFINITY;
        u8 val8;
        f32 val32;
        for (s32 zi = 0; zi < kernel; zi++)
          for (s32 yi = 0; yi < kernel; yi++)
            for (s32 xi = 0; xi < kernel; xi++) {
              if (z + zi > inchunk->depth || y + yi > inchunk->height || x + xi > inchunk->width) { continue; }
              if (inchunk->dtype == U8 && (val8 = chunk_get_u8(inchunk, z * stride + zi, y * stride + yi,
                                                               x * stride + xi)) > max8) { max8 = val8; }
              else if (inchunk->dtype == F32 && (val32 = chunk_get_f32(inchunk, z * stride + zi, y * stride + yi,
                                                                       x * stride + xi)) > max32) { max32 = val32; }
            }
        if (inchunk->dtype == U8) { chunk_set_u8(&ret, z, y, x, max8); }
        else if (inchunk->dtype == F32) { chunk_set_f32(&ret, z, y, x, max32); }
      }
  return ret;
}
*/
static chunk* avgpool(chunk* inchunk, s32 kernel, s32 stride) {
  s32 dims[3] = {(inchunk->dims[0] + stride - 1) / stride, (inchunk->dims[1] + stride - 1) / stride,
                        (inchunk->dims[2] + stride - 1) / stride};
  chunk* ret = chunk_new(dims);
  s32 len = kernel * kernel * kernel;
  s32 i = 0;
  f32* data = malloc(len * sizeof(f32));
  for (s32 z = 0; z < ret->dims[0]; z++)
    for (s32 y = 0; y < ret->dims[1]; y++)
      for (s32 x = 0; x < ret->dims[2]; x++) {
        len = kernel * kernel * kernel;
        i = 0;
        for (s32 zi = 0; zi < kernel; zi++)
          for (s32 yi = 0; yi < kernel; yi++)
            for (s32 xi = 0; xi < kernel; xi++) {
              if (z + zi > inchunk->dims[0] || y + yi > inchunk->dims[1] || x + xi > inchunk->dims[2]) {
                len--;
                continue;
              }
              data[i++] = chunk_at(inchunk, z * stride + zi, y * stride + yi, x * stride + xi);

            }
        chunk_set(ret, z, y, x, avgf32(data, len));
      }
  return ret;
}
/*
static chunk create_box_kernel(s32 size) {
  chunk kernel = chunk_new(F32, size, size, size);
  float value = 1.0f / (size * size * size);
  for (s32 z = 0; z < size; z++) {
    for (s32 y = 0; y < size; y++) { for (s32 x = 0; x < size; x++) { chunk_set_f32(&kernel, z, y, x, value); } }
  }
  return kernel;
}

static chunk convolve3d(chunk* input, chunk* kernel) {
  chunk output = chunk_new(input->dtype, input->depth, input->height, input->width);
  s32 pad = kernel->depth / 2;

  for (s32 z = 0; z < input->depth; z++) {
    for (s32 y = 0; y < input->height; y++) {
      for (s32 x = 0; x < input->width; x++) {
        float sum = 0.0f;
        for (s32 kz = 0; kz < kernel->depth; kz++) {
          for (s32 ky = 0; ky < kernel->height; ky++) {
            for (s32 kx = 0; kx < kernel->width; kx++) {
              s32 iz = z + kz - pad;
              s32 iy = y + ky - pad;
              s32 ix = x + kx - pad;
              if (iz >= 0 && iz < input->depth && iy >= 0 && iy < input->height && ix >= 0 && ix < input->width) {
                float input_val = (input->dtype == U8)
                                    ? (float)chunk_get_u8(input, iz, iy, ix)
                                    : chunk_get_f32(input, iz, iy, ix);
                sum += input_val * chunk_get_f32(kernel, kz, ky, kx);
              }
            }
          }
        }
        if (input->dtype == U8) { chunk_set_u8(&output, z, y, x, (u8)fminf(fmaxf(sum, 0), 255)); }
        else { chunk_set_f32(&output, z, y, x, sum); }
      }
    }
  }
  return output;
}

static chunk unsharp_mask_3d(chunk* input, float amount, s32 kernel_size) {
  chunk kernel = create_box_kernel(kernel_size);
  chunk blurred = convolve3d(input, &kernel);
  chunk output = chunk_new(input->dtype, input->depth, input->height, input->width);

  for (s32 z = 0; z < input->depth; z++) {
    for (s32 y = 0; y < input->height; y++) {
      for (s32 x = 0; x < input->width; x++) {
        float original, blur;
        if (input->dtype == U8) {
          original = (float)chunk_get_u8(input, z, y, x);
          blur = (float)chunk_get_u8(&blurred, z, y, x);
        }
        else {
          original = chunk_get_f32(input, z, y, x);
          blur = chunk_get_f32(&blurred, z, y, x);
        }

        float sharpened = original + amount * (original - blur);

        if (input->dtype == U8) { chunk_set_u8(&output, z, y, x, (u8)fminf(fmaxf(sharpened, 0), 255)); }
        else { chunk_set_f32(&output, z, y, x, sharpened); }
      }
    }
  }

  chunk_free(&kernel);
  chunk_free(&blurred);

  return output;
}
*/

static histogram* histogram_new(int num_bins, f32 min_value, f32 max_value) {
    histogram* hist = malloc(sizeof(histogram));
    if (!hist) {
        return NULL;
    }

    hist->bins = calloc(num_bins, sizeof(u32));
    if (!hist->bins) {
        free(hist);
        return NULL;
    }

    hist->num_bins = num_bins;
    hist->min_value = min_value;
    hist->max_value = max_value;
    hist->bin_width = (max_value - min_value) / num_bins;

    return hist;
}

static void histogram_free(histogram* hist) {
    if (hist) {
        free(hist->bins);
    }
}

static inline int get_bin_index(const histogram* hist, f32 value) {
    if (value <= hist->min_value) return 0;
    if (value >= hist->max_value) return hist->num_bins - 1;

    int bin = (int)((value - hist->min_value) / hist->bin_width);
    if (bin >= hist->num_bins) bin = hist->num_bins - 1;
    return bin;
}

static histogram* slice_histogram(const slice* slice, int num_bins) {
    if (!slice || num_bins <= 0) {
        return NULL;
    }

    f32 min_val = FLT_MAX;
    f32 max_val = -FLT_MAX;

    int total_pixels = slice->dims[0] * slice->dims[1];
    for (int i = 0; i < total_pixels; i++) {
        f32 val = slice->data[i];
        if (val < min_val) min_val = val;
        if (val > max_val) max_val = val;
    }

    histogram* hist = histogram_new(num_bins, min_val, max_val);
    if (!hist) {
        return NULL;
    }

    for (int i = 0; i < total_pixels; i++) {
        int bin = get_bin_index(hist, slice->data[i]);
        hist->bins[bin]++;
    }

    return hist;
}

static histogram* chunk_histogram(const chunk* chunk, int num_bins) {
    if (!chunk || num_bins <= 0) {
        return NULL;
    }

    f32 min_val = FLT_MAX;
    f32 max_val = -FLT_MAX;

    int total_voxels = chunk->dims[0] * chunk->dims[1] * chunk->dims[2];
    for (int i = 0; i < total_voxels; i++) {
        f32 val = chunk->data[i];
        if (val < min_val) min_val = val;
        if (val > max_val) max_val = val;
    }

    histogram* hist = histogram_new(num_bins, min_val, max_val);
    if (!hist) {
        return NULL;
    }

    for (int i = 0; i < total_voxels; i++) {
        int bin = get_bin_index(hist, chunk->data[i]);
        hist->bins[bin]++;
    }

    return hist;
}

static errcode write_histogram_to_csv(const histogram* hist, const char* filename) {
    FILE* fp = fopen(filename, "w");
    if (!fp) {
        return FAIL;
    }

    fprintf(fp, "bin_start,bin_end,count\n");

    for (int i = 0; i < hist->num_bins; i++) {
        f32 bin_start = hist->min_value + i * hist->bin_width;
        f32 bin_end = bin_start + hist->bin_width;
        fprintf(fp, "%.6f,%.6f,%u\n", bin_start, bin_end, hist->bins[i]);
    }

    fclose(fp);
    return SUCCESS;
}

static hist_stats calculate_histogram_stats(const histogram* hist) {
    hist_stats stats = {0};

    u64 total_count = 0;
    f64 weighted_sum = 0;
    u32 max_count = 0;

    for (int i = 0; i < hist->num_bins; i++) {
        f32 bin_center = hist->min_value + (i + 0.5f) * hist->bin_width;
        weighted_sum += bin_center * hist->bins[i];
        total_count += hist->bins[i];

        if (hist->bins[i] > max_count) {
            max_count = hist->bins[i];
            stats.mode = bin_center;
            stats.mode_count = hist->bins[i];
        }
    }

    stats.mean = (f32)(weighted_sum / total_count);

    f64 variance_sum = 0;
    for (int i = 0; i < hist->num_bins; i++) {
        f32 bin_center = hist->min_value + (i + 0.5f) * hist->bin_width;
        f32 diff = bin_center - stats.mean;
        variance_sum += diff * diff * hist->bins[i];
    }
    stats.std_dev = (f32)sqrt(variance_sum / total_count);

    u64 median_count = total_count / 2;
    u64 running_count = 0;
    for (int i = 0; i < hist->num_bins; i++) {
        running_count += hist->bins[i];
        if (running_count >= median_count) {
            stats.median = hist->min_value + (i + 0.5f) * hist->bin_width;
            break;
        }
    }

    return stats;
}
