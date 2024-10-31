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

#include "minilibs.h"



PUBLIC chunk *tiff_to_chunk(const char *tiffpath) {
  TiffImage *img = readTIFF(tiffpath);
  if (!img || !img->isValid) {
    assert(false);
    return NULL;
  }
  if (img->depth <= 1) {
    printf("can't load a 2d tiff as a chunk");
    assert(false);
    return NULL;
  }

  //TODO: can we assume that all 3D tiffs have the same x,y dimensions for all slices? because we are right here
  s32 dims[3] = {img->depth, img->directories[0].height, img->directories[0].width};
  chunk *ret = chunk_new(dims);
  for (s32 z = 0; z < dims[0]; z++) {
    void *buf = readTIFFDirectoryData(img, z);
    for (s32 y = 0; y < dims[1]; y++) {
      for (s32 x = 0; x < dims[2]; x++) {
        if (img->directories[z].bitsPerSample == 8) {
          ret->data[z * dims[1] * dims[2] + y * dims[2] + x] = getTIFFPixel8FromBuffer(
            buf, y, x, img->directories[z].width);
        } else if (img->directories[z].bitsPerSample == 16) {
          ret->data[z * dims[1] * dims[2] + y * dims[2] + x] = getTIFFPixel16FromBuffer(
            buf, y, x, img->directories[z].width);
        }
      }
    }
  }
  return ret;
}


PUBLIC slice *tiff_to_slice(const char *tiffpath, int index) {
  TiffImage *img = readTIFF(tiffpath);
  if (!img || !img->isValid) {
    assert(false);
    return NULL;
  }
  if (index < 0 || index >= img->depth) {
    assert(false);
    return NULL;
  }

  s32 dims[2] = {img->directories[0].height, img->directories[0].width};
  slice *ret = slice_new(dims);

  void *buf = readTIFFDirectoryData(img, index);
  for (s32 y = 0; y < dims[0]; y++) {
    for (s32 x = 0; x < dims[1]; x++) {
      if (img->directories[index].bitsPerSample == 8) {
        ret->data[y * dims[1] + x] = getTIFFPixel8FromBuffer(buf, y, x, img->directories[index].width);
      } else if (img->directories[index].bitsPerSample == 16) {
        ret->data[y * dims[1] + x] = getTIFFPixel16FromBuffer(buf, y, x, img->directories[index].width);
      }
    }
  }
  return ret;
}



PUBLIC int slice_fill(slice *slice, volume *vol, int start[static 2], int axis) {
  assert(axis == 'z' || axis == 'y' || axis == 'x');
  if (start[0] + slice->dims[0] < 0 || start[0] + slice->dims[0] > vol->dims[0]) {
    assert(false);
    return 1;
  }
  if (start[1] + slice->dims[1] < 0 || start[1] + slice->dims[1] > vol->dims[1]) {
    assert(false);
    return 1;
  }

  for (int y = 0; y < vol->dims[0]; y++) {
    for (int x = 0; x < vol->dims[1]; x++) {
      //TODO: actually get the data
      slice->data[y * slice->dims[1] + x] = 0.0f;
    }
  }
  return 0;
}

PUBLIC int chunk_fill(chunk *chunk, volume *vol, int start[static 3]) {
  if (start[0] + chunk->dims[0] < 0 || start[0] + chunk->dims[0] > vol->dims[0]) {
    assert(false);
    return 1;
  }
  if (start[1] + chunk->dims[1] < 0 || start[1] + chunk->dims[1] > vol->dims[1]) {
    assert(false);
    return 1;
  }
  if (start[2] + chunk->dims[2] < 0 || start[2] + chunk->dims[2] > vol->dims[2]) {
    assert(false);
    return 1;
  }

  for (int z = 0; z < vol->dims[0]; z++) {
    for (int y = 0; y < vol->dims[1]; y++) {
      for (int x = 0; x < vol->dims[2]; x++) {
        //TODO: actually get the data
        chunk->data[z * chunk->dims[1] * chunk->dims[2] + y * chunk->dims[2] + x] = 0.0f;
      }
    }
  }
  return 0;
}

PUBLIC int easy_snic(chunk *mychunk, s32 density, f32 compactness, chunk **labelsout, Superpixel **superpixelsout) {
  s32 lz, ly, lx;
  lz = mychunk->dims[0];
  ly = mychunk->dims[1];
  lx = mychunk->dims[2];

  *labelsout = chunk_new(mychunk->dims);
  s32 superpixels_count = snic_superpixel_count(lx, ly, lz, density);
  *superpixelsout = calloc(superpixels_count, sizeof(Superpixel));
  return snic(mychunk->data, lx, ly, lz, density, compactness, 80.0f, 160.0f, (*labelsout)->data, *superpixelsout);
}
