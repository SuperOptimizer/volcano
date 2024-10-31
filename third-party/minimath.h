#pragma once

#include <ctype.h>
#include <limits.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <float.h>
#include <stdbool.h>
#include <assert.h>
#include <math.h>

#include "minilibs.h"

// A chunk is a 3d cross section of data
//   - this could be a 512x512x512 section starting at 2000x2000x2000 and ending at 2512 x 2512 x 2512
//   - the dtype is float32
//   - increasing Z means increasing through the slice. e.g. 1000.tif -> 1001.tif
//   - increasing Y means looking farther down in a slice
//   - increasing X means looking farther right in a slice
// A slice is a 2d cross section of data
//   - increasing Y means looking farther down in a slice
//   - increasing X means looking farther right in a slice


typedef struct chunk {
  int dims[3];
  float data[];
} chunk __attribute__((aligned(16)));

typedef struct slice {
  int dims[2];
  float data[];
} slice __attribute__((aligned(16)));

PRIVATE float maxfloat(float a, float b) { return a > b ? a : b; }
PRIVATE float minfloat(float a, float b) { return a < b ? a : b; }
PRIVATE float avgfloat(float *data, int len) {
  double sum = 0.0;
  for (int i = 0; i < len; i++) sum += data[i];
  return sum / len;
}

PUBLIC chunk *chunk_new(int dims[static 3]) {
  chunk *ret = malloc(sizeof(chunk) + dims[0] * dims[1] * dims[2] * sizeof(float));

  if (ret == NULL) {
    assert(false);
    return NULL;
  }

  for (int i = 0; i < 3; i++) {
    ret->dims[i] = dims[i];
  }
  return ret;
}

PUBLIC void chunk_free(chunk *chunk) {
  free(chunk);
}

PUBLIC slice *slice_new(int dims[static 2]) {
  slice *ret = malloc(sizeof(slice) + dims[0] * dims[1] * sizeof(float));

  if (ret == NULL) {
    assert(false);
    return NULL;
  }

  for (int i = 0; i < 2; i++) {
    ret->dims[i] = dims[i];
  }
  return ret;
}

PUBLIC void slice_free(slice *slice) {
  free(slice);
}

PUBLIC f32 slice_get(slice *slice, s32 y, s32 x) {
  return slice->data[y * slice->dims[1] + x];
}

PUBLIC void slice_set(slice *slice, s32 y, s32 x, f32 data) {
  slice->data[y * slice->dims[1] + x] = data;
}

PUBLIC f32 chunk_get(chunk *chunk, s32 z, s32 y, s32 x) {
  return chunk->data[z * chunk->dims[1] * chunk->dims[2] + y * chunk->dims[2] + x];
}

PUBLIC void chunk_set(chunk *chunk, s32 z, s32 y, s32 x, f32 data) {
  chunk->data[z * chunk->dims[1] * chunk->dims[2] + y * chunk->dims[2] + x] = data;
}


PUBLIC chunk* maxpool(chunk* inchunk, s32 kernel, s32 stride) {
  s32 dims[3] = {
    (inchunk->dims[0] + stride - 1) / stride, (inchunk->dims[1] + stride - 1) / stride,
    (inchunk->dims[2] + stride - 1) / stride
  };
  chunk *ret = chunk_new(dims);
  for (s32 z = 0; z < ret->dims[0]; z++)
    for (s32 y = 0; y < ret->dims[1]; y++)
      for (s32 x = 0; x < ret->dims[2]; x++) {
        f32 max32 = -INFINITY;
        f32 val32;
        for (s32 zi = 0; zi < kernel; zi++)
          for (s32 yi = 0; yi < kernel; yi++)
            for (s32 xi = 0; xi < kernel; xi++) {
              if (z + zi > inchunk->dims[0] || y + yi > inchunk->dims[1] || x + xi > inchunk->dims[2]) { continue; }

              if ((val32 = chunk_get(inchunk, z * stride + zi, y * stride + yi,
                                                                       x * stride + xi)) > max32) { max32 = val32; }
            }
        chunk_set(ret, z, y, x, max32);
      }
  return ret;
}

PUBLIC chunk *avgpool(chunk *inchunk, s32 kernel, s32 stride) {
  s32 dims[3] = {
    (inchunk->dims[0] + stride - 1) / stride, (inchunk->dims[1] + stride - 1) / stride,
    (inchunk->dims[2] + stride - 1) / stride
  };
  chunk *ret = chunk_new(dims);
  s32 len = kernel * kernel * kernel;
  s32 i = 0;
  f32 *data = malloc(len * sizeof(f32));
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
              data[i++] = chunk_get(inchunk, z * stride + zi, y * stride + yi, x * stride + xi);
            }
        chunk_set(ret, z, y, x, avgfloat(data, len));
      }
  return ret;
}

PUBLIC chunk *sumpool(chunk *inchunk, s32 kernel, s32 stride) {
  s32 dims[3] = {
    (inchunk->dims[0] + stride - 1) / stride, (inchunk->dims[1] + stride - 1) / stride,
    (inchunk->dims[2] + stride - 1) / stride
  };
  chunk *ret = chunk_new(dims);
  for (s32 z = 0; z < ret->dims[0]; z++)
    for (s32 y = 0; y < ret->dims[1]; y++)
      for (s32 x = 0; x < ret->dims[2]; x++) {
        f32 sum = 0.0f;
        for (s32 zi = 0; zi < kernel; zi++)
          for (s32 yi = 0; yi < kernel; yi++)
            for (s32 xi = 0; xi < kernel; xi++) {
              if (z + zi > inchunk->dims[0] || y + yi > inchunk->dims[1] || x + xi > inchunk->dims[2]) {
                continue;
              }
              sum += chunk_get(inchunk, z * stride + zi, y * stride + yi, x * stride + xi);
            }
        chunk_set(ret, z, y, x, sum);
      }
  return ret;
}


PRIVATE chunk *create_box_kernel(s32 size) {
  int dims[3] = {size,size,size};
  chunk* kernel = chunk_new(dims);
  float value = 1.0f / (size * size * size);
  for (s32 z = 0; z < size; z++) {
    for (s32 y = 0; y < size; y++) { for (s32 x = 0; x < size; x++) { chunk_set(kernel, z, y, x, value); } }
  }
  return kernel;
}

PRIVATE chunk* convolve3d(chunk* input, chunk* kernel) {

  s32 dims[3] = {input->dims[0], input->dims[1], input->dims[2]};

  chunk* ret = chunk_new(dims);
  s32 pad = kernel->dims[0] / 2;

  for (s32 z = 0; z < input->dims[0]; z++) {
    for (s32 y = 0; y < input->dims[1]; y++) {
      for (s32 x = 0; x < input->dims[2]; x++) {
        float sum = 0.0f;
        for (s32 kz = 0; kz < kernel->dims[0]; kz++) {
          for (s32 ky = 0; ky < kernel->dims[1]; ky++) {
            for (s32 kx = 0; kx < kernel->dims[2]; kx++) {
              s32 iz = z + kz - pad;
              s32 iy = y + ky - pad;
              s32 ix = x + kx - pad;
              if (iz >= 0 && iz < input->dims[0] && iy >= 0 && iy < input->dims[1] && ix >= 0 && ix < input->dims[2]) {
                float input_val = chunk_get(input, iz, iy, ix);
                sum += input_val * chunk_get(kernel, kz, ky, kx);
              }
            }
          }
        }
        chunk_set(ret, z, y, x, sum);
      }
    }
  }
  return ret;
}

PUBLIC chunk* unsharp_mask_3d(chunk* input, float amount, s32 kernel_size) {
  int dims[3] = {input->dims[0], input->dims[1], input->dims[2]};
  chunk* kernel = create_box_kernel(kernel_size);
  chunk* blurred = convolve3d(input, kernel);
  chunk* output = chunk_new(dims);

  for (s32 z = 0; z < input->dims[0]; z++) {
    for (s32 y = 0; y < input->dims[1]; y++) {
      for (s32 x = 0; x < input->dims[2]; x++) {
        float original = chunk_get(input, z, y, x);
        float blur = chunk_get(blurred, z, y, x);
        float sharpened = original + amount * (original - blur);
        chunk_set(output, z, y, x, sharpened);
      }
    }
  }

  chunk_free(kernel);
  chunk_free(blurred);

  return output;
}

PUBLIC chunk* normalize_chunk(chunk* input) {
  // Create output chunk with same dimensions
  int dims[3] = {input->dims[0], input->dims[1], input->dims[2]};
  chunk* output = chunk_new(dims);

  // First pass: find min and max values
  float min_val = INFINITY;
  float max_val = -INFINITY;

  for (s32 z = 0; z < input->dims[0]; z++) {
    for (s32 y = 0; y < input->dims[1]; y++) {
      for (s32 x = 0; x < input->dims[2]; x++) {
        float val = chunk_get(input, z, y, x);
        min_val = minfloat(min_val, val);
        max_val = maxfloat(max_val, val);
      }
    }
  }

  // Handle edge case where all values are the same
  float range = max_val - min_val;
  if (range == 0.0f) {
    for (s32 z = 0; z < input->dims[0]; z++) {
      for (s32 y = 0; y < input->dims[1]; y++) {
        for (s32 x = 0; x < input->dims[2]; x++) {
          chunk_set(output, z, y, x, 0.5f);
        }
      }
    }
    return output;
  }

  // Second pass: normalize values to [0.0, 1.0]
  for (s32 z = 0; z < input->dims[0]; z++) {
    for (s32 y = 0; y < input->dims[1]; y++) {
      for (s32 x = 0; x < input->dims[2]; x++) {
        float val = chunk_get(input, z, y, x);
        float normalized = (val - min_val) / range;
        chunk_set(output, z, y, x, normalized);
      }
    }
  }

  return output;
}

PUBLIC chunk* transpose(chunk* input, const char* current_layout) {
    if (!input || !current_layout || strlen(current_layout) != 3) {
        return NULL;
    }

    int mapping[3] = {0, 0, 0};

    for (int i = 0; i < 3; i++) {
        switch (current_layout[i]) {
            case 'z':
                mapping[0] = i;
                break;
            case 'y':
                mapping[1] = i;
                break;
            case 'x':
                mapping[2] = i;
                break;
            default:
                return NULL;  // Invalid
        }
    }

    int new_dims[3] = {
        input->dims[mapping[0]],  // z
        input->dims[mapping[1]],  // y
        input->dims[mapping[2]]   // x
    };

    chunk* output = chunk_new(new_dims);
    if (!output) {
        return NULL;
    }

    // Perform the transpose
    for (int z = 0; z < new_dims[0]; z++) {
        for (int y = 0; y < new_dims[1]; y++) {
            for (int x = 0; x < new_dims[2]; x++) {
                // Create index array for the input layout
                int idx[3] = {z, y, x};

                // Map the indices according to the input layout
                int old_indices[3] = {
                    idx[mapping[0]],  // map z position
                    idx[mapping[1]],  // map y position
                    idx[mapping[2]]   // map x position
                };

                float value = chunk_get(input, old_indices[0], old_indices[1], old_indices[2]);
                chunk_set(output, z, y, x, value);
            }
        }
    }

    return output;
}