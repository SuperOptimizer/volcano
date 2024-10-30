#pragma once
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "minilibs.h"

//TODO: most the ply files I come across use x y z order. should we swap the order here so they
// end up in the data as z y x?

PUBLIC int write_ply(const char *filename,
                    const float *vertices,
                    const float *normals, // can be NULL if no normals
                    const int *indices,
                    int vertex_count,
                    int index_count) {

  FILE *fp = fopen(filename, "w");
  if (!fp) {
    return 1;
  }

  fprintf(fp, "ply\n");
  fprintf(fp, "format ascii 1.0\n");
  fprintf(fp, "comment Created by minilibs\n");
  fprintf(fp, "element vertex %d\n", vertex_count);
  fprintf(fp, "property float x\n");
  fprintf(fp, "property float y\n");
  fprintf(fp, "property float z\n");

  if (normals) {
    fprintf(fp, "property float nx\n");
    fprintf(fp, "property float ny\n");
    fprintf(fp, "property float nz\n");
  }

  fprintf(fp, "element face %d\n", index_count / 3);
  fprintf(fp, "property list uchar int vertex_indices\n");
  fprintf(fp, "end_header\n");

  for (int i = 0; i < vertex_count; i++) {
    if (normals) {
      fprintf(fp, "%.6f %.6f %.6f %.6f %.6f %.6f\n",
              vertices[i * 3],     // x
              vertices[i * 3 + 1], // y
              vertices[i * 3 + 2], // z
              normals[i * 3],     // nx
              normals[i * 3 + 1], // ny
              normals[i * 3 + 2]  // nz
      );
    } else {
      fprintf(fp, "%.6f %.6f %.6f\n",
              vertices[i * 3],     // x
              vertices[i * 3 + 1], // y
              vertices[i * 3 + 2]  // z
      );
    }
  }

  // Write faces
  for (int i = 0; i < index_count; i += 3) {
    fprintf(fp, "3 %d %d %d\n",
            indices[i],
            indices[i + 1],
            indices[i + 2]);
  }

  fclose(fp);
  return 0;
}

PUBLIC int read_ply(const char *filename,
                          float **out_vertices,
                          float **out_normals,
                          int **out_indices,
                          int *out_vertex_count,
                          int *out_normal_count,
                          int *out_index_count) {
  FILE *fp = fopen(filename, "rb");
  if (!fp) {
    return 1;
  }

  char buffer[1024];
  if (!fgets(buffer, sizeof(buffer), fp) || strncmp(buffer, "ply", 3) != 0) {
    fclose(fp);
    return 1;
  }

  // Check format
  if (!fgets(buffer, sizeof(buffer), fp)) {
    fclose(fp);
    return 1;
  }
  int is_binary = (strncmp(buffer, "format binary_little_endian", 26) == 0);
  int is_ascii = (strncmp(buffer, "format ascii", 11) == 0);
  if (!is_binary && !is_ascii) {
    fclose(fp);
    return 1;
  }

  int vertex_count = 0;
  int face_count = 0;
  int has_normals = 0;
  int in_header = 1;
  int got_vertex = 0;
  int got_face = 0;
  int is_double = 0;  // Track if the file uses doubles

  // Parse header
  while (in_header && fgets(buffer, sizeof(buffer), fp)) {
    if (strncmp(buffer, "end_header", 10) == 0) {
      in_header = 0;
    } else if (strncmp(buffer, "element vertex", 13) == 0) {
      sscanf(buffer, "element vertex %d", &vertex_count);
      got_vertex = 1;
    } else if (strncmp(buffer, "element face", 12) == 0) {
      sscanf(buffer, "element face %d", &face_count);
      got_face = 1;
    } else if (strncmp(buffer, "property double", 14) == 0) {
      is_double = 1;  // File uses doubles
    } else if (strncmp(buffer, "property double nx", 17) == 0) {
      has_normals = 1;
    }
  }

  if (!got_vertex || vertex_count <= 0) {
    fclose(fp);
    return 1;
  }

  // Allocate memory for float32 output
  float *vertices = malloc(vertex_count * 3 * sizeof(float));
  float *normals = NULL;
  int *indices = NULL;

  if (has_normals) {
    normals = malloc(vertex_count * 3 * sizeof(float));
    if (!normals) {
      free(vertices);
      fclose(fp);
      return 1;
    }
  }

  if (got_face && face_count > 0) {
    indices = malloc(face_count * 3 * sizeof(int));
    if (!indices) {
      free(vertices);
      free(normals);
      fclose(fp);
      return 1;
    }
  }

  if (!vertices) {
    free(normals);
    free(indices);
    fclose(fp);
    return 1;
  }

  // Read vertex data
  if (is_binary) {
    if (is_double) {
      // Reading doubles and converting to floats
      double temp[6];  // Temporary buffer for doubles (3 for position, 3 for normals)
      for (int i = 0; i < vertex_count; i++) {
        // Read position as double and convert to float
        if (fread(temp, sizeof(double), 3, fp) != 3) {
          free(vertices);
          free(normals);
          free(indices);
          fclose(fp);
          return 1;
        }
        vertices[i * 3] = (float)temp[0];
        vertices[i * 3 + 1] = (float)temp[1];
        vertices[i * 3 + 2] = (float)temp[2];

        // Read normals if present
        if (has_normals) {
          if (fread(temp, sizeof(double), 3, fp) != 3) {
            free(vertices);
            free(normals);
            free(indices);
            fclose(fp);
            return 1;
          }
          normals[i * 3] = (float)temp[0];
          normals[i * 3 + 1] = (float)temp[1];
          normals[i * 3 + 2] = (float)temp[2];
        }
      }
    } else {
      // Reading floats directly
      for (int i = 0; i < vertex_count; i++) {
        // Read position
        if (fread(&vertices[i * 3], sizeof(float), 3, fp) != 3) {
          free(vertices);
          free(normals);
          free(indices);
          fclose(fp);
          return 1;
        }

        // Read normals if present
        if (has_normals) {
          if (fread(&normals[i * 3], sizeof(float), 3, fp) != 3) {
            free(vertices);
            free(normals);
            free(indices);
            fclose(fp);
            return 1;
          }
        }
      }
    }
  } else {
    // ASCII reading - read as double and convert to float
    double temp[6];  // Temporary buffer for doubles
    for (int i = 0; i < vertex_count; i++) {
      if (has_normals) {
        if (fscanf(fp, "%lf %lf %lf %lf %lf %lf",
                   &temp[0], &temp[1], &temp[2],
                   &temp[3], &temp[4], &temp[5]) != 6) {
          free(vertices);
          free(normals);
          free(indices);
          fclose(fp);
          return 1;
        }
        vertices[i * 3] = (float)temp[0];
        vertices[i * 3 + 1] = (float)temp[1];
        vertices[i * 3 + 2] = (float)temp[2];
        normals[i * 3] = (float)temp[3];
        normals[i * 3 + 1] = (float)temp[4];
        normals[i * 3 + 2] = (float)temp[5];
      } else {
        if (fscanf(fp, "%lf %lf %lf",
                   &temp[0], &temp[1], &temp[2]) != 3) {
          free(vertices);
          free(normals);
          free(indices);
          fclose(fp);
          return 1;
        }
        vertices[i * 3] = (float)temp[0];
        vertices[i * 3 + 1] = (float)temp[1];
        vertices[i * 3 + 2] = (float)temp[2];
      }
    }
  }

  // Read face data if present
  int index_count = 0;
  if (got_face && indices) {
    if (is_binary) {
      for (int i = 0; i < face_count; i++) {
        unsigned char vertex_per_face;
        if (fread(&vertex_per_face, sizeof(unsigned char), 1, fp) != 1 || vertex_per_face != 3) {
          free(vertices);
          free(normals);
          free(indices);
          fclose(fp);
          return 1;
        }

        if (fread(&indices[index_count], sizeof(int), 3, fp) != 3) {
          free(vertices);
          free(normals);
          free(indices);
          fclose(fp);
          return 1;
        }
        index_count += 3;
      }
    } else {
      for (int i = 0; i < face_count; i++) {
        int vertex_per_face;
        if (fscanf(fp, "%d", &vertex_per_face) != 1 || vertex_per_face != 3) {
          free(vertices);
          free(normals);
          free(indices);
          fclose(fp);
          return 1;
        }

        if (fscanf(fp, "%d %d %d",
                   &indices[index_count],
                   &indices[index_count + 1],
                   &indices[index_count + 2]) != 3) {
          free(vertices);
          free(normals);
          free(indices);
          fclose(fp);
          return 1;
        }
        index_count += 3;
      }
    }
  }

  fclose(fp);

  *out_vertices = vertices;
  *out_normals = normals;
  *out_indices = indices;
  *out_vertex_count = vertex_count;
  *out_normal_count = has_normals ? vertex_count : 0;
  *out_index_count = index_count;

  return 0;
}