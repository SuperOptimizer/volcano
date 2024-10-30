#pragma once
#include <ctype.h>
#include <limits.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "minilibs.h"



// PPM format types
typedef enum ppm_type {
    P3,  // ASCII format
    P6   // Binary format
} ppm_type;

typedef struct ppm_image {
    u32 width;
    u32 height;
    u8 max_val;
    u8* data;  // RGB data in row-major order
} ppm_image;

PUBLIC inline ppm_image* ppm_new(u32 width, u32 height) {
    ppm_image* img = malloc(sizeof(ppm_image));
    if (!img) {
        return NULL;
    }

    img->width = width;
    img->height = height;
    img->max_val = 255;
    img->data = calloc(width * height * 3, sizeof(u8));

    if (!img->data) {
        free(img);
        return NULL;
    }

    return img;
}

PUBLIC inline void ppm_free(ppm_image* img) {
    if (img) {
        free(img->data);
        free(img);
    }
}

PRIVATE void skip_whitespace_and_comments(FILE* fp) {
    int c;
    while ((c = fgetc(fp)) != EOF) {
        if (c == '#') {
            // Skip until end of line
            while ((c = fgetc(fp)) != EOF && c != '\n');
        } else if (!isspace(c)) {
            ungetc(c, fp);
            break;
        }
    }
}

PRIVATE bool read_header(FILE* fp, ppm_type* type, u32* width, u32* height, u8* max_val) {
    char magic[3];

    if (fgets(magic, sizeof(magic), fp) == NULL) {
        return false;
    }

    if (magic[0] != 'P' || (magic[1] != '3' && magic[1] != '6')) {
        return false;
    }

    *type = magic[1] == '3' ? P3 : P6;

    skip_whitespace_and_comments(fp);

    if (fscanf(fp, "%u %u", width, height) != 2) {
        return false;
    }

    skip_whitespace_and_comments(fp);

    unsigned int max_val_temp;
    if (fscanf(fp, "%u", &max_val_temp) != 1 || max_val_temp > 255) {
        return false;
    }
    *max_val = (u8)max_val_temp;

    fgetc(fp);

    return true;
}

PUBLIC ppm_image* read_ppm(const char* filename) {
    FILE* fp = fopen(filename, "rb");
    if (!fp) {
        return NULL;
    }

    ppm_type type;
    u32 width, height;
    u8 max_val;

    if (!read_header(fp, &type, &width, &height, &max_val)) {
        fclose(fp);
        return NULL;
    }

    ppm_image* img = ppm_new(width, height);
    if (!img) {
        fclose(fp);
        return NULL;
    }

    img->max_val = max_val;
    size_t pixel_count = width * height * 3;

    if (type == P3) {
        // ASCII format
        for (size_t i = 0; i < pixel_count; i++) {
            unsigned int val;
            if (fscanf(fp, "%u", &val) != 1 || val > max_val) {
                ppm_free(img);
                fclose(fp);
                return NULL;
            }
            img->data[i] = (u8)val;
        }
    } else {
        // Binary format
        if (fread(img->data, 1, pixel_count, fp) != pixel_count) {
            ppm_free(img);
            fclose(fp);
            fclose(fp);
            return NULL;
        }
    }

    fclose(fp);
    return img;
}

PUBLIC int write_ppm(const char* filename, const ppm_image* img, ppm_type type) {
    if (!img || !img->data) {
        return 1;
    }

    FILE* fp = fopen(filename, "wb");
    if (!fp) {
        return 1;
    }

    // Write header
    fprintf(fp, "P%c\n", type == P3 ? '3' : '6');
    fprintf(fp, "%u %u\n", img->width, img->height);
    fprintf(fp, "%u\n", img->max_val);

    size_t pixel_count = img->width * img->height * 3;

    if (type == P3) {
        // ASCII format
        for (size_t i = 0; i < pixel_count; i++) {
            fprintf(fp, "%u", img->data[i]);
            fprintf(fp, (i + 1) % 3 == 0 ? "\n" : " ");
        }
    } else {
        // Binary format
        if (fwrite(img->data, 1, pixel_count, fp) != pixel_count) {
            fclose(fp);
            return 1;
        }
    }

    fclose(fp);
    return 0;
}

PRIVATE void ppm_set_pixel(ppm_image* img, u32 x, u32 y, u8 r, u8 g, u8 b) {
    if (!img || x >= img->width || y >= img->height) {
        return;
    }

    size_t idx = (y * img->width + x) * 3;
    img->data[idx] = r;
    img->data[idx + 1] = g;
    img->data[idx + 2] = b;
}

PRIVATE void ppm_get_pixel(const ppm_image* img, u32 x, u32 y, u8* r, u8* g, u8* b) {
    if (!img || x >= img->width || y >= img->height) {
        *r = *g = *b = 0;
        return;
    }

    size_t idx = (y * img->width + x) * 3;
    *r = img->data[idx];
    *g = img->data[idx + 1];
    *b = img->data[idx + 2];
}