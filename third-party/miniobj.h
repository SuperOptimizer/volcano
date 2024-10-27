#pragma once

#include <ctype.h>
#include <limits.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <assert.h>



// Reader function that takes pointers to all components
int read_obj(const char* filename,
            float** vertices, int** indices,
            int* vertex_count, int* index_count) {
    FILE* fp = fopen(filename, "r");
    if (!fp) {
        return 1;
    }

    // Initial allocations
    size_t vertex_capacity = 1024;
    size_t index_capacity = 1024;
    *vertices = malloc(vertex_capacity * 3 * sizeof(float));
    *indices = malloc(index_capacity * sizeof(int));
    *vertex_count = 0;
    *index_count = 0;

    char line[256];
    while (fgets(line, sizeof(line), fp)) {
        if (line[0] == 'v' && line[1] == ' ') {
            // Reallocate vertices if needed
            if (*vertex_count >= vertex_capacity) {
                vertex_capacity *= 2;
                float* new_vertices = realloc(*vertices, vertex_capacity * 3 * sizeof(float));
                if (!new_vertices) {
                    fclose(fp);
                    return 1;
                }
                *vertices = new_vertices;
            }

            // Read vertex coordinates
            float x, y, z;
            if (sscanf(line + 2, "%f %f %f", &x, &y, &z) == 3) {
                (*vertices)[(*vertex_count) * 3] = x;
                (*vertices)[(*vertex_count) * 3 + 1] = y;
                (*vertices)[(*vertex_count) * 3 + 2] = z;
                (*vertex_count)++;
            }
        }
        else if (line[0] == 'f' && line[1] == ' ') {
            // Parse face indices
            int v1, v2, v3, t1, t2, t3, n1, n2, n3;
            int matches = sscanf(line + 2, "%d/%d/%d %d/%d/%d %d/%d/%d",
                               &v1, &t1, &n1, &v2, &t2, &n2, &v3, &t3, &n3);

            if (matches != 9) {
                // Try parsing without texture/normal indices
                matches = sscanf(line + 2, "%d %d %d", &v1, &v2, &v3);
                if (matches != 3) {
                    continue;  // Skip malformed faces
                }
            }

            // Reallocate indices if needed
            if (*index_count + 3 > index_capacity) {
                index_capacity *= 2;
                int* new_indices = realloc(*indices, index_capacity * sizeof(int));
                if (!new_indices) {
                    fclose(fp);
                    return 1;
                }
                *indices = new_indices;
            }

            // Store face indices (converting from 1-based to 0-based)
            (*indices)[(*index_count)++] = v1 - 1;
            (*indices)[(*index_count)++] = v2 - 1;
            (*indices)[(*index_count)++] = v3 - 1;
        }
    }

    // Shrink arrays to actual size
    *vertices = realloc(*vertices, (*vertex_count) * 3 * sizeof(float));
    *indices = realloc(*indices, (*index_count) * sizeof(int));

    fclose(fp);
    return 0;
}

// Writer function that takes the components directly
int write_obj(const char* filename,
             const float* vertices, const int* indices,
             int vertex_count, int index_count) {
    FILE* fp = fopen(filename, "w");
    if (!fp) {
        return 1;
    }

    // Write header comment
    fprintf(fp, "# OBJ file created by mesh writer\n");

    // Write vertices
    for (int i = 0; i < vertex_count; i++) {
        fprintf(fp, "v %.6f %.6f %.6f\n",
                vertices[i * 3],
                vertices[i * 3 + 1],
                vertices[i * 3 + 2]);
    }

    // Write faces (converting from 0-based to 1-based indices)
    assert(index_count % 3 == 0);  // Ensure we have complete triangles
    for (int i = 0; i < index_count; i += 3) {
        fprintf(fp, "f %d %d %d\n",
                indices[i] + 1,
                indices[i + 1] + 1,
                indices[i + 2] + 1);
    }

    fclose(fp);
    return 0;
}