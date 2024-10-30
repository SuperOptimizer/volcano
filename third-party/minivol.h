#pragma once

#include "minilibs.h"


typedef struct volume {
  s32 dims[3];
  bool is_zarr;
  bool is_tif_stack;
  bool uses_3d_tif;
  bool local_storage;
} volume;

PUBLIC volume *volume_new(s32 dims[static 3], bool is_zarr, bool is_tif_stack, bool uses_3d_tif, bool local_storage) {
  assert(!(is_zarr && (is_tif_stack || uses_3d_tif)));

  volume *ret = malloc(sizeof(volume));
  if (ret == NULL) {
    assert(false);
    return NULL;
  }

  *ret = (volume){{dims[0], dims[1], dims[2]}, is_zarr, is_tif_stack, uses_3d_tif, local_storage};
  return ret;
}