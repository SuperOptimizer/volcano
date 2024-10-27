#pragma once
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


static inline int write_mesh_to_ply(const char *filename,
                                    const float *vertices,
                                    const int *indices,
                                    int vertex_count,
                                    int index_count) {
  FILE *fp = fopen(filename, "w");
  if (!fp) {
    return 1; // Error
  }

  fprintf(fp, "ply\n");
  fprintf(fp, "format ascii 1.0\n");
  fprintf(fp, "comment Created by marching cubes implementation\n");
  fprintf(fp, "element vertex %d\n", vertex_count);
  fprintf(fp, "property float x\n");
  fprintf(fp, "property float y\n");
  fprintf(fp, "property float z\n");
  fprintf(fp, "element face %d\n", index_count / 3);
  fprintf(fp, "property list uchar int vertex_indices\n");
  fprintf(fp, "end_header\n");

  // Write vertices - now accessing from flat array where each vertex is 3 consecutive floats
  for (int i = 0; i < vertex_count; i++) {
    fprintf(fp, "%.6f %.6f %.6f\n",
            vertices[i * 3], // x
            vertices[i * 3 + 1], // y
            vertices[i * 3 + 2] // z
    );
  }

  // Write faces - same as before since indices were already flat
  for (int i = 0; i < index_count; i += 3) {
    fprintf(fp, "3 %d %d %d\n",
            indices[i],
            indices[i + 1],
            indices[i + 2]);
  }

  fclose(fp);
  return 0; // Success
}


static void skip_line(FILE *fp) {
  char buffer[1024];
  fgets(buffer, sizeof(buffer), fp);
}

static inline int read_mesh_from_ply(const char *filename,
                                     float **out_vertices, // Will be allocated
                                     int **out_indices, // Will be allocated
                                     int *out_vertex_count,
                                     int *out_index_count) {
  FILE *fp = fopen(filename, "r");
  if (!fp) {
    return 1;
  }

  // Check PLY header
  char buffer[1024];
  if (!fgets(buffer, sizeof(buffer), fp) || strncmp(buffer, "ply", 3) != 0) {
    fclose(fp);
    return 1;
  }

  // Parse header
  int vertex_count = 0;
  int face_count = 0;
  int in_header = 1;
  int got_vertex = 0;
  int got_face = 0;

  while (in_header && fgets(buffer, sizeof(buffer), fp)) {
    if (strncmp(buffer, "end_header", 10) == 0) {
      in_header = 0;
    } else if (strncmp(buffer, "element vertex", 13) == 0) {
      sscanf(buffer, "element vertex %d", &vertex_count);
      got_vertex = 1;
    } else if (strncmp(buffer, "element face", 12) == 0) {
      sscanf(buffer, "element face %d", &face_count);
      got_face = 1;
    }
  }

  if (!got_vertex || !got_face || vertex_count <= 0 || face_count <= 0) {
    fclose(fp);
    return 1;
  }

  // Allocate arrays
  float *vertices = malloc(vertex_count * 3 * sizeof(float));
  int *indices = malloc(face_count * 3 * sizeof(int)); // Assuming triangles

  if (!vertices || !indices) {
    free(vertices);
    free(indices);
    fclose(fp);
    return 1;
  }

  // Read vertices
  for (int i = 0; i < vertex_count; i++) {
    if (fscanf(fp, "%f %f %f",
               &vertices[i * 3],
               &vertices[i * 3 + 1],
               &vertices[i * 3 + 2]) != 3) {
      free(vertices);
      free(indices);
      fclose(fp);
      return 1;
    }
  }

  // Read faces
  int index_count = 0;
  for (int i = 0; i < face_count; i++) {
    int vertex_per_face;
    if (fscanf(fp, "%d", &vertex_per_face) != 1) {
      free(vertices);
      free(indices);
      fclose(fp);
      return 1;
    }

    if (vertex_per_face != 3) {
      // We only support triangles
      free(vertices);
      free(indices);
      fclose(fp);
      return 1;
    }

    // Read the three vertex indices
    if (fscanf(fp, "%d %d %d",
               &indices[index_count],
               &indices[index_count + 1],
               &indices[index_count + 2]) != 3) {
      free(vertices);
      free(indices);
      fclose(fp);
      return 1;
    }
    index_count += 3;
  }

  fclose(fp);

  *out_vertices = vertices;
  *out_indices = indices;
  *out_vertex_count = vertex_count;
  *out_index_count = index_count;

  return 0;
}
