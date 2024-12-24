#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>
#include "volcano.h"
#include "snic.h"
#include "chord.h"

#define ZLIB_CHUNK_SIZE 16384

// Helper function to compress a string using zlib
static int compress_string(const char* input, size_t input_len, char** output, size_t* output_len) {
    z_stream strm;
    char out[ZLIB_CHUNK_SIZE];
    char* compressed = NULL;
    size_t total_size = 0;

    strm.zalloc = Z_NULL;
    strm.zfree = Z_NULL;
    strm.opaque = Z_NULL;

    if (deflateInit2(&strm, Z_BEST_COMPRESSION, Z_DEFLATED, 31, 8, Z_DEFAULT_STRATEGY) != Z_OK) {
        return -1;
    }

    strm.avail_in = input_len;
    strm.next_in = (unsigned char*)input;

    do {
        strm.avail_out = ZLIB_CHUNK_SIZE;
        strm.next_out = (unsigned char*)out;

        if (deflate(&strm, Z_FINISH) == Z_STREAM_ERROR) {
            deflateEnd(&strm);
            free(compressed);
            return -1;
        }

        size_t have = ZLIB_CHUNK_SIZE - strm.avail_out;
        char* new_compressed = realloc(compressed, total_size + have);
        if (!new_compressed) {
            deflateEnd(&strm);
            free(compressed);
            return -1;
        }
        compressed = new_compressed;
        memcpy(compressed + total_size, out, have);
        total_size += have;
    } while (strm.avail_out == 0);

    deflateEnd(&strm);
    *output = compressed;
    *output_len = total_size;
    return 0;
}

// Helper function to decompress a string using zlib
static int decompress_string(const char* input, size_t input_len, char** output, size_t* output_len) {
    z_stream strm;
    char out[ZLIB_CHUNK_SIZE];
    char* decompressed = NULL;
    size_t total_size = 0;

    strm.zalloc = Z_NULL;
    strm.zfree = Z_NULL;
    strm.opaque = Z_NULL;
    strm.avail_in = 0;
    strm.next_in = Z_NULL;

    if (inflateInit2(&strm, 31) != Z_OK) {
        return -1;
    }

    strm.avail_in = input_len;
    strm.next_in = (unsigned char*)input;

    do {
        strm.avail_out = ZLIB_CHUNK_SIZE;
        strm.next_out = (unsigned char*)out;

        int ret = inflate(&strm, Z_NO_FLUSH);
        if (ret != Z_OK && ret != Z_STREAM_END) {
            inflateEnd(&strm);
            free(decompressed);
            return -1;
        }

        size_t have = ZLIB_CHUNK_SIZE - strm.avail_out;
        char* new_decompressed = realloc(decompressed, total_size + have);
        if (!new_decompressed) {
            inflateEnd(&strm);
            free(decompressed);
            return -1;
        }
        decompressed = new_decompressed;
        memcpy(decompressed + total_size, out, have);
        total_size += have;
    } while (strm.avail_out == 0);

    inflateEnd(&strm);
    *output = decompressed;
    *output_len = total_size;
    return 0;
}

// Save superpixels to compressed CSV
static int superpixels_to_compressed_csv(char* path, Superpixel* superpixels, int num_superpixels) {
    // First, write to memory
    char* csv_data = NULL;
    size_t csv_size = 0;
    FILE* memfile = open_memstream(&csv_data, &csv_size);
    if (!memfile) return -1;

    // Write header
    fprintf(memfile, "z,y,x,intensity,pixel_count\n");

    // Write data
    for (int i = 0; i < num_superpixels; i++) {
        fprintf(memfile, "%.1f,%.1f,%.1f,%.1f,%u\n",
                superpixels[i].z,
                superpixels[i].y,
                superpixels[i].x,
                superpixels[i].c,
                superpixels[i].n);
    }

    fclose(memfile);

    // Compress the data
    char* compressed_data;
    size_t compressed_size;
    if (compress_string(csv_data, csv_size, &compressed_data, &compressed_size) != 0) {
        free(csv_data);
        return -1;
    }

    // Write compressed data to file
    FILE* fp = fopen(path, "wb");
    if (!fp) {
        free(csv_data);
        free(compressed_data);
        return -1;
    }

    size_t written = fwrite(compressed_data, 1, compressed_size, fp);
    fclose(fp);

    free(csv_data);
    free(compressed_data);

    return written == compressed_size ? 0 : -1;
}

// Load superpixels from compressed CSV
static Superpixel* compressed_csv_to_superpixels(char* path, int* num_superpixels_out) {
    // Read compressed file
    FILE* fp = fopen(path, "rb");
    if (!fp) return NULL;

    fseek(fp, 0, SEEK_END);
    size_t file_size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    char* compressed_data = malloc(file_size);
    if (!compressed_data) {
        fclose(fp);
        return NULL;
    }

    if (fread(compressed_data, 1, file_size, fp) != file_size) {
        free(compressed_data);
        fclose(fp);
        return NULL;
    }
    fclose(fp);

    // Decompress data
    char* csv_data;
    size_t csv_size;
    if (decompress_string(compressed_data, file_size, &csv_data, &csv_size) != 0) {
        free(compressed_data);
        return NULL;
    }
    free(compressed_data);

    // Parse CSV from memory
    char* line = csv_data;
    char* end = csv_data + csv_size;
    int num_lines = 0;

    // Skip header
    while (line < end && *line != '\n') line++;
    line++; // Skip the newline

    // Count lines
    char* counting_line = line;
    while (counting_line < end) {
        if (*counting_line++ == '\n') num_lines++;
    }

    // Allocate array
    Superpixel* superpixels = calloc(num_lines, sizeof(Superpixel));
    if (!superpixels) {
        free(csv_data);
        return NULL;
    }

    // Read data
    int i = 0;
    while (line < end && i < num_lines) {
        float z, y, x, c;
        unsigned int n;

        if (sscanf(line, "%f,%f,%f,%f,%u", &z, &y, &x, &c, &n) == 5) {
            superpixels[i].z = z;
            superpixels[i].y = y;
            superpixels[i].x = x;
            superpixels[i].c = c;
            superpixels[i].n = n;
            i++;
        }

        // Move to next line
        while (line < end && *line != '\n') line++;
        line++; // Skip the newline
    }

    free(csv_data);
    *num_superpixels_out = num_lines;
    return superpixels;
}


// Save superpixels to CSV
static int superpixels_to_csv(char* path, Superpixel* superpixels, int num_superpixels) {
    FILE* fp = fopen(path, "w");
    if (!fp) return -1;

    // Write header
    fprintf(fp, "z,y,x,intensity,pixel_count\n");

    // Write data
    for (int i = 0; i < num_superpixels; i++) {
        fprintf(fp, "%.1f,%.1f,%.1f,%.1f,%u\n",
                superpixels[i].z,
                superpixels[i].y,
                superpixels[i].x,
                superpixels[i].c,
                superpixels[i].n);
    }

    fclose(fp);
    return 0;
}

// Load superpixels from CSV
static Superpixel* csv_to_superpixels(char* path, int* num_superpixels_out) {
    FILE* fp = fopen(path, "r");
    if (!fp) return NULL;

    char line[1024];
    int num_lines = 0;

    // Skip header
    fgets(line, sizeof(line), fp);

    // Count lines
    while (fgets(line, sizeof(line), fp)) {
        num_lines++;
    }

    // Allocate array
    Superpixel* superpixels = calloc(num_lines, sizeof(Superpixel));
    if (!superpixels) {
        fclose(fp);
        return NULL;
    }

    // Reset file pointer and skip header again
    fseek(fp, 0, SEEK_SET);
    fgets(line, sizeof(line), fp);

    // Read data
    int i = 0;
    while (fgets(line, sizeof(line), fp)) {
        float z, y, x, c;
        unsigned int n;

        if (sscanf(line, "%f,%f,%f,%f,%u",
                   &z, &y, &x, &c, &n) == 5) {
            superpixels[i].z = z;
            superpixels[i].y = y;
            superpixels[i].x = x;
            superpixels[i].c = c;
            superpixels[i].n = n;
            i++;
        }
    }

    fclose(fp);
    *num_superpixels_out = num_lines;
    return superpixels;
}

// Save chords to CSV - just saves the list of superpixel indices
static int chords_to_csv(char* path, Chord* chords, int num_chords) {
    FILE* fp = fopen(path, "w");
    if (!fp) return -1;

    // Write header
    fprintf(fp, "points\n");

    // Write data - each line is a comma-separated list of points
    for (int i = 0; i < num_chords; i++) {
        for (int j = 0; j < chords[i].point_count; j++) {
            fprintf(fp, "%u", chords[i].points[j]);
            if (j < chords[i].point_count - 1) fprintf(fp, ",");
        }
        fprintf(fp, "\n");
    }

    fclose(fp);
    return 0;
}

// Load chords from CSV
static Chord* csv_to_chords(char* path, int* num_chords_out) {
    FILE* fp = fopen(path, "r");
    if (!fp) return NULL;

    char line[16384];  // Large buffer for long point lists
    int num_lines = 0;

    // Skip header
    fgets(line, sizeof(line), fp);

    // Count lines
    while (fgets(line, sizeof(line), fp)) {
        num_lines++;
    }

    // Allocate array
    Chord* chords = calloc(num_lines, sizeof(Chord));
    if (!chords) {
        fclose(fp);
        return NULL;
    }

    // Reset file pointer and skip header
    fseek(fp, 0, SEEK_SET);
    fgets(line, sizeof(line), fp);

    // Read data
    int i = 0;
    while (fgets(line, sizeof(line), fp)) {
        // First pass: count points in this line
        int point_count = 1;  // Start at 1 for the first number
        for (char* c = line; *c; c++) {
            if (*c == ',') point_count++;
        }

        // Allocate points array
        chords[i].points = malloc(point_count * sizeof(uint32_t));
        chords[i].point_count = point_count;

        // Parse points
        char* point_start = line;
        int point_idx = 0;
        while (*point_start && *point_start != '\n') {
            char* end;
            uint32_t point = strtoul(point_start, &end, 10);
            if (end == point_start) break;

            if (point_idx < point_count) {
                chords[i].points[point_idx++] = point;
            }

            point_start = end;
            while (*point_start && (*point_start == ',' || *point_start == ' ')) {
                point_start++;
            }
        }

        i++;
    }

    fclose(fp);
    *num_chords_out = num_lines;
    return chords;
}

// Write chords with full superpixel data to CSV
static int chords_with_data_to_csv(const char* path,
                                  const Chord* chords,
                                  int num_chords,
                                  const Superpixel* superpixels) {
    FILE* fp = fopen(path, "w");
    if (!fp) return -1;

    // Write header with all fields
    fprintf(fp, "chord_id,superpixel_id,z,y,x,intensity,pixel_count\n");

    // Write data - each line contains full information about each point in the chord
    for (int i = 0; i < num_chords; i++) {
        const Chord* chord = &chords[i];

        for (int j = 0; j < chord->point_count; j++) {
            uint32_t superpixel_id = chord->points[j];
            const Superpixel* sp = &superpixels[superpixel_id];

            // Write: chord_id, point_index, superpixel_id, superpixel data (z,y,x,intensity,count), position data
            fprintf(fp, "%d,%u,%.1f,%.1f,%.1f,%.1f,%u\n",
                    i,                    // chord_id
                    superpixel_id,        // original superpixel id
                    sp->z,                // superpixel centroid z
                    sp->y,                // superpixel centroid y
                    sp->x,                // superpixel centroid x
                    sp->c,                // intensity
                    sp->n                // pixel count
            );
        }
    }

    fclose(fp);
    return 0;
}

// Read chords with full data from CSV
static Chord* csv_to_chords_with_data(const char* path, int* num_chords_out) {
    FILE* fp = fopen(path, "r");
    if (!fp) return NULL;

    char line[1024];
    int max_chords = 1024; // Initial capacity
    Chord* chords = calloc(max_chords, sizeof(Chord));
    if (!chords) {
        fclose(fp);
        return NULL;
    }

    // Skip header
    fgets(line, sizeof(line), fp);

    int current_chord_id = -1;
    int num_chords = 0;
    Chord* current_chord = NULL;
    int point_capacity = 0;

    // Read data line by line
    while (fgets(line, sizeof(line), fp)) {
        int chord_id;
        uint32_t superpixel_id;
        float sp_z, sp_y, sp_x, intensity;
        unsigned int pixel_count;

        if (sscanf(line, "%d,%u,%f,%f,%f,%f,%u",
                   &chord_id, &superpixel_id,
                   &sp_z, &sp_y, &sp_x, &intensity, &pixel_count) != 8) {
            continue;
        }

        // If this is a new chord
        if (chord_id != current_chord_id) {
            current_chord_id = chord_id;

            // Expand chords array if needed
            if (chord_id >= max_chords) {
                int new_max = max_chords * 2;
                Chord* new_chords = realloc(chords, new_max * sizeof(Chord));
                if (!new_chords) {
                    for (int i = 0; i < num_chords; i++) {
                        free(chords[i].points);
                        free(chords[i].recent_dirs);
                    }
                    free(chords);
                    fclose(fp);
                    return NULL;
                }
                chords = new_chords;
                max_chords = new_max;
            }

            current_chord = &chords[chord_id];
            point_capacity = 128; // Initial point capacity
            current_chord->points = malloc(point_capacity * sizeof(uint32_t));
            current_chord->recent_dirs = malloc(MAX_RECENT_DIRS * NUM_DIMENSIONS * sizeof(float));
            current_chord->point_count = 0;
            current_chord->num_recent_dirs = 0;

            if (chord_id >= num_chords) {
                num_chords = chord_id + 1;
            }
        }
    }

    fclose(fp);
    *num_chords_out = num_chords;
    return chords;
}