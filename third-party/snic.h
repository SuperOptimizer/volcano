#pragma once

// This comes from https://github.com/spelufo/stabia/tree/main
// licensed under the MIT license
// Modified by VerditeLabs

/*
MIT License

Copyright (c) 2023 Santiago Pelufo

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>


// HEAP ////////////////////////////////////////////////////////////////////////

// The only part of this Heap implementation specific to SNIC are the HeapNode
// and heap_node_val definitions. The heap_node_val is the heap value i.e. the
// priority, and it is the SNIC distance negated, because this is a max-heap
// and the SNIC algorithm wants a min-heap.

typedef struct HeapNode {
  f32 d;
  u32 k;
  u16 x, y, z;
  u16 pad;
} HeapNode;

#define heap_node_val(n)  (-n.d)

typedef struct Heap {
  int len, size;
  HeapNode* nodes;
} Heap;

#define heap_left(i)  (2*(i))
#define heap_right(i) (2*(i)+1)
#define heap_parent(i) ((i)/2)
#define heap_fix_edge(heap, i, j) \
  if (heap_node_val(heap->nodes[j]) > heap_node_val(heap->nodes[i])) { \
    HeapNode tmp = heap->nodes[j]; \
    heap->nodes[j] = heap->nodes[i]; \
    heap->nodes[i] = tmp; \
  }

static Heap heap_alloc(int size) {
  return (Heap){.len = 0, .size = size, .nodes = (HeapNode*)calloc(size*2+1, sizeof(HeapNode))};
}

static void heap_free(Heap *heap) {
  free(heap->nodes);
}

static void heap_push(Heap *heap, HeapNode node) {
  //assert(heap->len <= heap->size);

  heap->len++;
  heap->nodes[heap->len] = node;
  for (int i = heap->len, j = 0; i > 1; i = j) {
    j = heap_parent(i);
    heap_fix_edge(heap, j, i) else break;
  }
}

static HeapNode heap_pop(Heap *heap) {
  //assert(heap->len > 0);

  HeapNode node = heap->nodes[1];
  heap->len--;
  heap->nodes[1] = heap->nodes[heap->len+1];
  for (int i = 1, j = 0; i <= heap->len; i = j) {
    int l = heap_left(i);
    int r = heap_right(i);
    if (l > heap->len) {
      break;
    }
    j = l;
    if (r <= heap->len && heap_node_val(heap->nodes[l]) < heap_node_val(heap->nodes[r])) {
      j = r;
    } else {
    }
    heap_fix_edge(heap, i, j) else break;
  }

  return node;
}

#undef heap_left
#undef heap_right
#undef heap_parent
#undef heap_fix_edge


// SNIC ////////////////////////////////////////////////////////////////////////

// This is based on the paper and the code from:
// - https://www.epfl.ch/labs/ivrl/research/snic-superpixels/
// - https://github.com/achanta/SNIC/

// There isn't a theoretical maximum for SNIC neighbors. The neighbors of a cube
// would be 26, so if compactness is high we shouldn't exceed that by too much.
// 56 results in sizeof(Superpixel) == 4*8*8 (4 64B cachelines).
#define SUPERPIXEL_MAX_NEIGHS (56*2)
typedef struct Superpixel {
  f32 x, y, z, c;
  u32 n, nlow, nmid, nhig;
  u32 neighs[SUPERPIXEL_MAX_NEIGHS];
} Superpixel;

static int snic_superpixel_max_neighs()  {
  return SUPERPIXEL_MAX_NEIGHS;
}

static int superpixel_add_neighbors(Superpixel *superpixels, u32 k1, u32 k2) {
  int i = 0;
  for (; i < SUPERPIXEL_MAX_NEIGHS; i++) {
    if (superpixels[k1].neighs[i] == 0) {
      superpixels[k1].neighs[i] = k2;
      return 0;
    } else if (superpixels[k1].neighs[i] == k2) {
      return 0;
    }
  }
  return 1;
}

static int snic_superpixel_count(int lx, int ly, int lz, int d_seed)  {
  int cz = (lz - d_seed/2 + d_seed - 1)/d_seed;
  int cy = (ly - d_seed/2 + d_seed - 1)/d_seed;
  int cx = (lx - d_seed/2 + d_seed - 1)/d_seed;
  return cx*cy*cz;
}

// The labels must be the same size as img, and all zeros.
static int snic(f32 *img, int lz, int ly, int lx, int d_seed, f32 compactness, f32 lowmid, f32 midhig, u32 *labels, Superpixel* superpixels) {
  int neigh_overflow = 0; // Number of neighbors that couldn't be added.
  int lylx = ly * lx;
  int img_size = lylx * lz;
  #define idx(z, y, x) ((z)*lylx + (x)*ly + (y))
  #define sqr(x) ((x)*(x))

  // Initialize priority queue with seeds on a grid with step d_seed.
  Heap pq = heap_alloc(img_size*16);
  u32 numk = 0;
  for (u16 iz = d_seed/2; iz < lz; iz += d_seed) {
    for (u16 iy = d_seed/2; iy < ly; iy += d_seed) {
      for (u16 ix = d_seed/2; ix < lx; ix += d_seed) {
        numk++;
        // Move seeds away from edges. Not essential but should improve results.
        u16 x = ix, y = iy, z = iz;
        f32 grad = INFINITY;
        for (u16 dz = -1; dz <= 1; dz++) {
          for (u16 dy = -1; dy <= 1; dy++) {
            for (u16 dx = -1; dx <= 1; dx++) {
              u16 jx = ix+dx, jy = iy+dy, jz = iz+dz;
              if (0 < jx && jx < lx-1 && 0 < jy && jy < ly-1 && 0 < jz && jz < lz-1) {
                f32 gy = img[idx(jz,jy+1,jx)] - img[idx(jz,jy-1,jx)];
                f32 gx = img[idx(jz,jy,jx+1)] - img[idx(jz,jy,jx-1)];
                f32 gz = img[idx(jz+1,jy,jx)] - img[idx(jz-1,jy,jx)];
                f32 jgrad = sqr(gx)+sqr(gy)+sqr(gz);
                if (jgrad < grad) {
                  x = jx; y = jy; z = jz;
                  grad = jgrad;
                }
              }
            }
          }
        }
        heap_push(&pq, (HeapNode){.d = 0.0f, .k = numk, .x = x, .y = y, .z = z});
      }
    }
  }
  // assert(numk == snic_superpixels_count(lx, ly, lz, d_seed));
  if (numk == 0) {
    return 0;
  }

  f32 invwt = (compactness*compactness*numk)/(f32)(img_size);

  while (pq.len > 0) {
    HeapNode n = heap_pop(&pq);
    int i = idx(n.z, n.y, n.x);
    if (labels[i] > 0) continue;

    u32 k = n.k;
    labels[i] = k;
    superpixels[k].c += img[i];
    superpixels[k].x += n.x;
    superpixels[k].y += n.y;
    superpixels[k].z += n.z;
    superpixels[k].n += 1;
    if      (img[i] <= lowmid) superpixels[k].nlow += 1;
    else if (img[i] <= midhig) superpixels[k].nmid += 1;
    else                       superpixels[k].nhig += 1;

    #define do_neigh(ndz, ndy, ndx, ioffset) { \
      int xx = n.x + ndx; int yy = n.y + ndy; int zz = n.z + ndz; \
      if (0 <= xx && xx < lx && 0 <= yy && yy < ly && 0 <= zz && zz < lz) { \
        int ii = i + ioffset; \
        if (labels[ii] <= 0) { \
          f32 ksize = (f32)superpixels[k].n; \
          f32 dc = sqr(100.0f*(superpixels[k].c - (img[ii]*ksize))); \
          f32 dx = superpixels[k].x - xx*ksize; \
          f32 dy = superpixels[k].y - yy*ksize; \
          f32 dz = superpixels[k].z - zz*ksize; \
          f32 dpos = sqr(dx) + sqr(dy) + sqr(dz); \
          f32 d = (dc + dpos*invwt) / (ksize*ksize); \
          heap_push(&pq, (HeapNode){.d = d, .k = k, .x = (u16)xx, .y = (u16)yy, .z = (u16)zz}); \
        } else if (k != labels[ii]) { \
          neigh_overflow += superpixel_add_neighbors(superpixels, k, labels[ii]); \
          neigh_overflow += superpixel_add_neighbors(superpixels, labels[ii], k); \
        } \
      } \
    }

    do_neigh( 0,  1,  0,    1);
    do_neigh( 0, -1,  0,   -1);
    do_neigh( 0,  0,  1,    ly);
    do_neigh( 0,  0, -1,   -ly);
    do_neigh( 1,  0,  0,  lylx);
    do_neigh(-1,  0,  0, -lylx);
    #undef do_neigh
  }

  for (u32 k = 1; k <= numk; k++) {
    f32 ksize = (f32)superpixels[k].n;
    superpixels[k].c /= ksize;
    superpixels[k].x /= ksize;
    superpixels[k].y /= ksize;
    superpixels[k].z /= ksize;
  }

  #undef sqr
  #undef idx
  heap_free(&pq);
  return neigh_overflow;
}

