#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "miniutils.h"

// Helper function to handle data conversion and reading
static int read_binary_data(FILE* fp, void* out_data, const char* src_type, const char* dst_type, size_t count) {
    // Fast path: types match, direct read
    if (strcmp(src_type, dst_type) == 0) {
        size_t element_size = strcmp(src_type, "float") == 0 ? sizeof(float) : sizeof(double);
        return fread(out_data, element_size, count, fp) == count ? 0 : 1;
    }

    // Conversion path
    if (strcmp(src_type, "double") == 0 && strcmp(dst_type, "float") == 0) {
        // Reading doubles, converting to floats
        double* temp = malloc(count * sizeof(double));
        if (!temp) return 1;

        int status = fread(temp, sizeof(double), count, fp) == count ? 0 : 1;
        if (status == 0) {
            float* out = (float*)out_data;
            for (size_t i = 0; i < count; i++) {
                out[i] = (float)temp[i];
            }
        }
        free(temp);
        return status;
    }
    else if (strcmp(src_type, "float") == 0 && strcmp(dst_type, "double") == 0) {
        // Reading floats, converting to doubles
        float* temp = malloc(count * sizeof(float));
        if (!temp) return 1;

        int status = fread(temp, sizeof(float), count, fp) == count ? 0 : 1;
        if (status == 0) {
            double* out = (double*)out_data;
            for (size_t i = 0; i < count; i++) {
                out[i] = (double)temp[i];
            }
        }
        free(temp);
        return status;
    }

    return 1;
}

// Helper function to handle data conversion and writing
static inline int write_binary_data(FILE* fp, const void* data, const char* src_type, const char* dst_type, size_t count) {
    // Fast path: types match, direct write
    if (strcmp(src_type, dst_type) == 0) {
        size_t element_size = strcmp(src_type, "float") == 0 ? sizeof(float) : sizeof(double);
        return fwrite(data, element_size, count, fp) == count ? 0 : 1;
    }

    // Conversion path
    if (strcmp(src_type, "float") == 0 && strcmp(dst_type, "double") == 0) {
        // Converting floats to doubles
        double* temp = malloc(count * sizeof(double));
        if (!temp) return 1;

        const float* in = (const float*)data;
        for (size_t i = 0; i < count; i++) {
            temp[i] = (double)in[i];
        }

        int status = fwrite(temp, sizeof(double), count, fp) == count ? 0 : 1;
        free(temp);
        return status;
    }
    else if (strcmp(src_type, "double") == 0 && strcmp(dst_type, "float") == 0) {
        // Converting doubles to floats
        float* temp = malloc(count * sizeof(float));
        if (!temp) return 1;

        const double* in = (const double*)data;
        for (size_t i = 0; i < count; i++) {
            temp[i] = (float)in[i];
        }

        int status = fwrite(temp, sizeof(float), count, fp) == count ? 0 : 1;
        free(temp);
        return status;
    }

    return 1;
}



static inline int read_vcps(const char* filename,
              size_t* width, size_t* height, size_t* dim,
              void* data, const char* dst_type) {
    if (!dst_type || (strcmp(dst_type, "float") != 0 && strcmp(dst_type, "double") != 0)) {
        fprintf(stderr, "Error: Invalid destination type\n");
        return 1;
    }

    FILE* fp = fopen(filename, "rb");
    if (!fp) {
        fprintf(stderr, "Error: Cannot open file %s\n", filename);
        return 1;
    }

    // Read header
    char line[256];
    int header_complete = 0;
    int ordered = 0;
    char src_type[32] = {0};
    int version = 0;
    *width = 0;
    *height = 0;
    *dim = 0;

    while (fgets(line, sizeof(line), fp)) {
        trim(line);

        if (strcmp(line, "<>") == 0) {
            header_complete = 1;
            break;
        }

        char key[32], value[32];
        if (sscanf(line, "%31[^:]: %31s", key, value) == 2) {
            if (strcmp(key, "width") == 0) {
                *width = atoi(value);
            } else if (strcmp(key, "height") == 0) {
                *height = atoi(value);
            } else if (strcmp(key, "dim") == 0) {
                *dim = atoi(value);
            } else if (strcmp(key, "type") == 0) {
                strncpy(src_type, value, sizeof(src_type) - 1);
            } else if (strcmp(key, "version") == 0) {
                version = atoi(value);
            } else if (strcmp(key, "ordered") == 0) {
                ordered = (strcmp(value, "true") == 0);
            }
        }
    }

    if (!header_complete || *width == 0 || *height == 0 || *dim == 0 ||
        (strcmp(src_type, "float") != 0 && strcmp(src_type, "double") != 0) ||
        !ordered) {
        fprintf(stderr, "Error: Invalid header (w=%zu h=%zu d=%zu t=%s o=%d)\n",
                *width, *height, *dim, src_type, ordered);
        fclose(fp);
        return 1;
    }

    size_t total_points = (*width) * (*height) * (*dim);
    int status = read_binary_data(fp, data, src_type, dst_type, total_points);

    fclose(fp);
    return status;
}

static inline int write_vcps(const char* filename,
               size_t width, size_t height, size_t dim,
               const void* data, const char* src_type, const char* dst_type) {
    if (!src_type || !dst_type ||
        (strcmp(src_type, "float") != 0 && strcmp(src_type, "double") != 0) ||
        (strcmp(dst_type, "float") != 0 && strcmp(dst_type, "double") != 0)) {
        fprintf(stderr, "Error: Invalid type specification\n");
        return 1;
    }

    FILE* fp = fopen(filename, "w");
    if (!fp) return 1;

    // Write header
    fprintf(fp, "width: %zu\n", width);
    fprintf(fp, "height: %zu\n", height);
    fprintf(fp, "dim: %zu\n", dim);
    fprintf(fp, "ordered: true\n");
    fprintf(fp, "type: %s\n", dst_type);
    fprintf(fp, "version: 1\n");
    fprintf(fp, "<>\n");
    fclose(fp);

    // Reopen in binary append mode for data
    fp = fopen(filename, "ab");
    if (!fp) return 1;

    size_t total_points = width * height * dim;
    int status = write_binary_data(fp, data, src_type, dst_type, total_points);

    fclose(fp);
    return status;
}
