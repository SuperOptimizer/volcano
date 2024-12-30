#pragma once

#include "volcano.h"

// Get neighbors for a 3D point, returns number of valid neighbors
int get_neighbors_3d(int z, int y, int x, int depth, int height, int width,
                    int* neighbor_coords) {
    const int directions[6][3] = {
        {-1, 0, 0}, {1, 0, 0},  // up/down
        {0, -1, 0}, {0, 1, 0},  // front/back
        {0, 0, -1}, {0, 0, 1}   // left/right
    };

    int count = 0;

    for (int i = 0; i < 6; i++) {
        int nz = z + directions[i][0];
        int ny = y + directions[i][1];
        int nx = x + directions[i][2];

        if (nz >= 0 && nz < depth && ny >= 0 && ny < height && nx >= 0 && nx < width) {
            neighbor_coords[count * 3] = nz;
            neighbor_coords[count * 3 + 1] = ny;
            neighbor_coords[count * 3 + 2] = nx;
            count++;
        }
    }

    return count;
}

// Flood fill implementation for float32 data
void flood_fill_f32(const float* volume, uint8_t* mask, uint8_t* visited,
                   int depth, int height, int width,
                   float iso_threshold, float start_threshold) {
    int max_size = depth * height * width;

    // Simple queue arrays
    int* queue_z = (int*)malloc(max_size * sizeof(int));
    int* queue_y = (int*)malloc(max_size * sizeof(int));
    int* queue_x = (int*)malloc(max_size * sizeof(int));
    int queue_start = 0;
    int queue_end = 0;

    // Find starting points
    for (int z = 0; z < depth; z++) {
        for (int y = 0; y < height; y++) {
            for (int x = 0; x < width; x++) {
                int idx = z * (height * width) + y * width + x;
                if (volume[idx] >= start_threshold) {
                    queue_z[queue_end] = z;
                    queue_y[queue_end] = y;
                    queue_x[queue_end] = x;
                    queue_end++;
                    mask[idx] = 1;
                    visited[idx] = 1;
                }
            }
        }
    }

    // Array for neighbor coordinates (max 6 neighbors * 3 coordinates)
    int neighbor_coords[18];

    // Process queue
    while (queue_start < queue_end) {
        int current_z = queue_z[queue_start];
        int current_y = queue_y[queue_start];
        int current_x = queue_x[queue_start];
        queue_start++;

        int neighbor_count = get_neighbors_3d(current_z, current_y, current_x,
                                           depth, height, width,
                                           neighbor_coords);

        for (int i = 0; i < neighbor_count; i++) {
            int z = neighbor_coords[i * 3];
            int y = neighbor_coords[i * 3 + 1];
            int x = neighbor_coords[i * 3 + 2];
            int idx = z * (height * width) + y * width + x;

            if (visited[idx] || volume[idx] < iso_threshold) {
                continue;
            }

            mask[idx] = 1;
            visited[idx] = 1;
            queue_z[queue_end] = z;
            queue_y[queue_end] = y;
            queue_x[queue_end] = x;
            queue_end++;
        }
    }

    free(queue_z);
    free(queue_y);
    free(queue_x);
}

float* segment_and_clean_f32(const float* volume, int depth, int height, int width,
                           float iso_threshold, float start_threshold) {
    int total_size = depth * height * width;

    // Allocate arrays
    uint8_t* mask = (uint8_t*)calloc(total_size, sizeof(uint8_t));
    uint8_t* visited = (uint8_t*)calloc(total_size, sizeof(uint8_t));
    float* result = (float*)calloc(total_size, sizeof(float));

    // Perform flood fill
    flood_fill_f32(volume, mask, visited, depth, height, width,
                   iso_threshold, start_threshold);

    // Apply mask to create result
    for (int i = 0; i < total_size; i++) {
        result[i] = mask[i] ? volume[i] : 0.0f;
    }

    // Clean up
    free(mask);
    free(visited);

    return result;
}

chunk *vs_avgpool_denoise(chunk *inchunk, s32 kernel) {
    // Create output chunk with same dimensions as input
    chunk *ret = vs_chunk_new(inchunk->dims);

    // Calculate kernel half-size for centered window
    s32 half = kernel / 2;

    // Pre-allocate buffer for storing neighborhood values
    s32 max_len = kernel * kernel * kernel;
    f32 *data = malloc(max_len * sizeof(f32));

    // Process each voxel in the volume
    for (s32 z = 0; z < inchunk->dims[0]; z++) {
        for (s32 y = 0; y < inchunk->dims[1]; y++) {
            for (s32 x = 0; x < inchunk->dims[2]; x++) {
                s32 count = 0;  // Track number of valid neighbors

                // Gather values from kernel-sized neighborhood
                for (s32 zi = -half; zi <= half; zi++) {
                    for (s32 yi = -half; yi <= half; yi++) {
                        for (s32 xi = -half; xi <= half; xi++) {
                            // Calculate neighbor coordinates
                            s32 nz = z + zi;
                            s32 ny = y + yi;
                            s32 nx = x + xi;

                            // Skip if out of bounds
                            if (nz < 0 || nz >= inchunk->dims[0] ||
                                ny < 0 || ny >= inchunk->dims[1] ||
                                nx < 0 || nx >= inchunk->dims[2]) {
                                continue;
                                }

                            // Add valid neighbor to data array
                            data[count++] = vs_chunk_get(inchunk, nz, ny, nx);
                        }
                    }
                }

                // Set output voxel to average of neighborhood
                vs_chunk_set(ret, z, y, x, vs__avgfloat(data, count));
            }
        }
    }

    free(data);
    return ret;
}