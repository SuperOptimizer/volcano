#pragma once

#include "minilibs.h"

// A volume is an entire scroll
//   - for Scroll 1 it is all 14376 x 7888 x 8096 voxels
//   - the dtype is uint8 or uint16
//

typedef struct volume {
  s32 dims[3];
  bool is_zarr;
  bool is_tif_stack;
  bool uses_3d_tif;
  char* cache_dir;
  u64 vol_id;
} volume;

PUBLIC volume *volume_new(s32 dims[static 3], bool is_zarr, bool is_tif_stack, bool uses_3d_tif, char* cache_dir, u64 vol_id) {
  //only 3d tiff chunks are currently supported
  assert(is_tif_stack && uses_3d_tif);

  volume *ret = malloc(sizeof(volume));
  if (ret == NULL) {
    assert(false);
    return NULL;
  }

  if (cache_dir != NULL) {
    if (mkdir_p(cache_dir)) {
      printf("Could not mkdir %s\n",cache_dir);
      return NULL;
    }
  }

  *ret = (volume){{dims[0], dims[1], dims[2]}, is_zarr, is_tif_stack, uses_3d_tif, cache_dir, vol_id};
  return ret;
}

PUBLIC chunk* volume_get_chunk(volume* vol, s32 chunk_pos[static 3], s32 chunk_dims[static 3]) {
  // stitching across chunks, be them tiff or zarr chunks, isn't just yet supported
  // so for now we'll just force the position to be a multiple of 500
  // and for dims to be <= 500

  assert(chunk_pos[0] % 500 == 0);
  assert(chunk_pos[1] % 500 == 0);
  assert(chunk_pos[2] % 500 == 0);
  assert(chunk_dims[0] <= 500);
  assert(chunk_dims[1] <= 500);
  assert(chunk_dims[2] <= 500);

  int z = chunk_pos[0] / 500;
  int y = chunk_pos[1] / 500;
  int x = chunk_pos[2] / 500;

  char filename[1024] = {'\0'};
  sprintf(filename, "cell_yxz_%03d_%03d_%03d.tif",y,x,z);
  char url[1024] = {'\0'};
  sprintf(url, "https://dl.ash2txt.org/full-scrolls/Scroll1/PHercParis4.volpkg/volume_grids/20230205180739/%s",filename);
  void* buf;
  printf("downloading data from %s\n",url);

  long len = download(url, &buf);
  printf("len %d\n",(s32)len);
  //assert(len == 250073508 );

  if (vol->cache_dir != NULL) {
    char outpath[1024] = {'\0'};
    sprintf(outpath, "%s/%s",vol->cache_dir, filename);
    printf("saving data to %s\n",outpath);
    FILE* fp = fopen(outpath, "wb");
    if (fp == NULL) {
      printf("could not open %s for writing\n",outpath);
      return NULL;
    }
    size_t sz = fwrite(buf, len, 1, fp);
    printf("wrote: %lld bytes to %s\n",sz, outpath);
    if (sz != len) {
      printf("could not write all data to %s\n",outpath);
    }
  }
  return NULL;
}