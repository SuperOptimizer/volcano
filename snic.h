#pragma once

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>

constexpr f32 compactness = 512.0f;
constexpr int d_seed = 2;
constexpr int dimension = 128;

typedef struct HeapNode {
  f32 d;
  u32 k;
  u8 z, y, x;
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

static inline Heap heap_alloc(int size) {
  return (Heap){.len = 0, .size = size, .nodes = (HeapNode*)calloc(size*2+1, sizeof(HeapNode))};
}

static inline void heap_free(Heap *heap) {
  free(heap->nodes);
}

static inline void heap_push(Heap *heap, HeapNode node) {
  heap->len++;
  heap->nodes[heap->len] = node;
  for (int i = heap->len, j = 0; i > 1; i = j) {
    j = heap_parent(i);
    heap_fix_edge(heap, j, i) else break;
  }
}

static inline HeapNode heap_pop(Heap *heap) {
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
    }
    heap_fix_edge(heap, i, j) else break;
  }
  return node;
}

#define SUPERPIXEL_MAX_NEIGHS (56)
typedef struct Superpixel {
  f32 z, y, x, c;
  u32 n;
} Superpixel;

#define snic_superpixel_count() ((dimension/2)*(dimension/2)*(dimension/2))

static int snic(f32 *img, u32 *labels, Superpixel* superpixels) {
  constexpr int lz = dimension;
  constexpr int ly = dimension;
  constexpr int lx = dimension;
  constexpr int lylx = ly * lx;
  constexpr int img_size = lylx * lz;

  // Initialize all labels to UINT32_MAX (uninitialized)
  for (int i = 0; i < img_size; i++) {
    labels[i] = UINT32_MAX;
  }

  constexpr f32 invwt = (compactness*compactness*snic_superpixel_count())/(f32)(img_size);

  #define idx(z, y, x) ((z)*lylx + (x)*ly + (y))
  #define sqr(x) ((x)*(x))

  int neigh_overflow = 0;
  Heap pq = heap_alloc(img_size);
  u32 numk = 0;

  for (u8 z = 0; z < lz; z += d_seed) {
    for (u8 y = 0; y < ly; y += d_seed) {
      for (u8 x = 0; x < lx; x += d_seed) {
        heap_push(&pq, (HeapNode){.d = 0.0f, .k = numk, .x = x, .y = y, .z = z});
        numk++;
      }
    }
  }

  while (pq.len > 0) {
    HeapNode n = heap_pop(&pq);
    int i = idx(n.z, n.y, n.x);
    if (labels[i] != UINT32_MAX) continue;

    u32 k = n.k;
    labels[i] = k;
    int c = img[i];
    superpixels[k].c += c;
    superpixels[k].x += n.x;
    superpixels[k].y += n.y;
    superpixels[k].z += n.z;
    superpixels[k].n += 1;

    #define do_neigh(ndz, ndy, ndx, ioffset) { \
      int xx = n.x + ndx; int yy = n.y + ndy; int zz = n.z + ndz; \
      if (0 <= xx && xx < lx && 0 <= yy && yy < ly && 0 <= zz && zz < lz) { \
        int ii = i + ioffset; \
        if (labels[ii] == UINT32_MAX) { \
          f32 ksize = (f32)superpixels[k].n; \
          f32 dc = sqr(255.0f*(superpixels[k].c - (img[ii]*ksize))); \
          f32 dx = superpixels[k].x - xx*ksize; \
          f32 dy = superpixels[k].y - yy*ksize; \
          f32 dz = superpixels[k].z - zz*ksize; \
          f32 dpos = sqr(dx) + sqr(dy) + sqr(dz); \
          f32 d = (dc + dpos*invwt) / (ksize*ksize); \
          heap_push(&pq, (HeapNode){.d = d, .k = k, .x = (u16)xx, .y = (u16)yy, .z = (u16)zz}); \
        } \
      } \
    }

    do_neigh( 0,  1,  0,    1);
    do_neigh( 0, -1,  0,   -1);
    do_neigh( 0,  0,  1,    ly);
    do_neigh( 0,  0, -1,   -ly);
    do_neigh( 1,  0,  0,  lylx);
    do_neigh(-1,  0,  0, -lylx);
  }

  for (u32 k = 0; k < snic_superpixel_count(); k++) {
    f32 ksize = (f32)superpixels[k].n;
    superpixels[k].c /= ksize;
    superpixels[k].x /= ksize;
    superpixels[k].y /= ksize;
    superpixels[k].z /= ksize;
  }

  heap_free(&pq);
  return neigh_overflow;
}

typedef struct SuperpixelConnection {
    u32 neighbor_label;
    f32 connection_strength;
} SuperpixelConnection;

typedef struct SuperpixelConnections {
    SuperpixelConnection* connections;
    int num_connections;
} SuperpixelConnections;

static void free_superpixel_connections(SuperpixelConnections* connections, u32 num_superpixels) {
    if (!connections) return;
    for (u32 i = 0; i < num_superpixels; i++) {
        SuperpixelConnection* conns = connections[i].connections;
        if (conns) {
            free(conns);
        }
    }
    free(connections);
}

static SuperpixelConnections* calculate_superpixel_connections(
    const f32* img,
    const u32* labels,
    int num_superpixels
) {
    constexpr int lz = dimension;
    constexpr int ly = dimension;
    constexpr int lx = dimension;
    constexpr int lylx = ly * lx;

    SuperpixelConnections* all_connections = calloc(num_superpixels, sizeof(SuperpixelConnections));
    if (!all_connections) return NULL;

    // First pass: count unique neighbors
    for (int z = 0; z < lz; z++) {
        for (int y = 0; y < ly; y++) {
            for (int x = 0; x < lx; x++) {
                u32 current_label = labels[idx(z,y,x)];
                if (current_label == UINT32_MAX) continue;

                for (int dz = -1; dz <= 1; dz++) {
                    for (int dy = -1; dy <= 1; dy++) {
                        for (int dx = -1; dx <= 1; dx++) {
                            if (dz == 0 && dy == 0 && dx == 0) continue;

                            int xx = x + dx;
                            int yy = y + dy;
                            int zz = z + dz;

                            if (xx < 0 || xx >= lx || yy < 0 || yy >= ly || zz < 0 || zz >= lz)
                                continue;

                            u32 neighbor_label = labels[idx(zz,yy,xx)];
                            if (neighbor_label == UINT32_MAX || neighbor_label == current_label)
                                continue;

                            bool found = false;
                            if (all_connections[current_label].connections) {
                                for (int i = 0; i < all_connections[current_label].num_connections; i++) {
                                    if (all_connections[current_label].connections[i].neighbor_label == neighbor_label) {
                                        found = true;
                                        break;
                                    }
                                }
                            }
                            if (!found) {
                                all_connections[current_label].num_connections++;
                            }
                        }
                    }
                }
            }
        }
    }

    // Allocate connection arrays
    for (u32 i = 0; i < num_superpixels; i++) {
        if (all_connections[i].num_connections > 0) {
            all_connections[i].connections = calloc(
                all_connections[i].num_connections, sizeof(SuperpixelConnection));
            all_connections[i].num_connections = 0;  // Reset for second pass
        }
    }

    // Second pass: calculate connections
    for (int z = 0; z < lz; z++) {
        for (int y = 0; y < ly; y++) {
            for (int x = 0; x < lx; x++) {
                u32 current_label = labels[idx(z,y,x)];
                if (current_label == UINT32_MAX) continue;
                float current_val = img[idx(z,y,x)];

                for (int dz = -1; dz <= 1; dz++) {
                    for (int dy = -1; dy <= 1; dy++) {
                        for (int dx = -1; dx <= 1; dx++) {
                            if (dz == 0 && dy == 0 && dx == 0) continue;

                            int xx = x + dx;
                            int yy = y + dy;
                            int zz = z + dz;

                            if (xx < 0 || xx >= lx || yy < 0 || yy >= ly || zz < 0 || zz >= lz)
                                continue;

                            u32 neighbor_label = labels[idx(zz,yy,xx)];
                            if (neighbor_label == UINT32_MAX || neighbor_label == current_label)
                                continue;

                            float neighbor_val = img[idx(zz,yy,xx)];
                            float value_similarity = 1.0f - fabsf(current_val - neighbor_val) / 255.0f;

                            int conn_idx = -1;
                            for (int i = 0; i < all_connections[current_label].num_connections; i++) {
                                if (all_connections[current_label].connections[i].neighbor_label == neighbor_label) {
                                    conn_idx = i;
                                    break;
                                }
                            }

                            if (conn_idx == -1) {
                                conn_idx = all_connections[current_label].num_connections++;
                                all_connections[current_label].connections[conn_idx].neighbor_label = neighbor_label;
                            }

                            all_connections[current_label].connections[conn_idx].connection_strength += value_similarity;
                        }
                    }
                }
            }
        }
    }

    return all_connections;
}

static int filter_superpixels(u32* labels, Superpixel* superpixels, int min_size, f32 min_val) {
    constexpr int lz = dimension;
    constexpr int ly = dimension;
    constexpr int lx = dimension;
    constexpr int lylx = ly * lx;
    constexpr int img_size = lylx * lz;

    int new_count = 0;
    u32* label_map = calloc(snic_superpixel_count(), sizeof(u32));

    for (u32 k = 0; k < snic_superpixel_count(); k++) {
        if (superpixels[k].n >= min_size && superpixels[k].c >= min_val) {
            label_map[k] = new_count;
            if (new_count != k) {
                superpixels[new_count] = superpixels[k];
            }
            new_count++;
        } else {
            label_map[k] = UINT32_MAX;
        }
    }

    for (int i = 0; i < img_size; i++) {
        if (labels[i] != UINT32_MAX) {
            labels[i] = label_map[labels[i]];
        }
    }

    free(label_map);
    return new_count;
}

#undef sqr
#undef idx