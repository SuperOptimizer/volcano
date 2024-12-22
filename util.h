#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "volcano.h"
#include "snic.h"
#include "chord.h"

// Save superpixels to CSV
static int superpixels_to_csv(char* path, Superpixel* superpixels, int num_superpixels) {
    FILE* fp = fopen(path, "w");
    if (!fp) return -1;

    // Write header
    fprintf(fp, "z,y,x,intensity,pixel_count\n");

    // Write data
    for (int i = 0; i < num_superpixels; i++) {
        fprintf(fp, "%f,%f,%f,%f,%u\n",
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