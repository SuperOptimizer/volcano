#pragma once


#include <ctype.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "minilibs.h"


typedef struct histogram {
  int num_bins;
  float min_value;
  float max_value;
  float bin_width;
  unsigned int *bins;
} histogram;

typedef struct hist_stats {
  float mean;
  float median;
  float mode;
  unsigned int mode_count;
  float std_dev;
} hist_stats;


PUBLIC histogram *histogram_new(int num_bins, float min_value, float max_value) {
  histogram *hist = malloc(sizeof(histogram));
  if (!hist) {
    return NULL;
  }

  hist->bins = calloc(num_bins, sizeof(unsigned int));
  if (!hist->bins) {
    free(hist);
    return NULL;
  }

  hist->num_bins = num_bins;
  hist->min_value = min_value;
  hist->max_value = max_value;
  hist->bin_width = (max_value - min_value) / num_bins;

  return hist;
}

PUBLIC void histogram_free(histogram *hist) {
  if (hist) {
    free(hist->bins);
    free(hist);
  }
}


PRIVATE int get_bin_index(const histogram* hist, float value) {
    if (value <= hist->min_value) return 0;
    if (value >= hist->max_value) return hist->num_bins - 1;

    int bin = (int)((value - hist->min_value) / hist->bin_width);
    if (bin >= hist->num_bins) bin = hist->num_bins - 1;
    return bin;
}

PUBLIC histogram* slice_histogram(const float* data,
                                      int dimy, int dimx,
                                      int num_bins) {
    if (!data || num_bins <= 0) {
        return NULL;
    }

    float min_val = FLT_MAX;
    float max_val = -FLT_MAX;

    int total_pixels = dimy * dimx;
    for (int i = 0; i < total_pixels; i++) {
        float val = data[i];
        if (val < min_val) min_val = val;
        if (val > max_val) max_val = val;
    }

    histogram* hist = histogram_new(num_bins, min_val, max_val);
    if (!hist) {
        return NULL;
    }

    for (int i = 0; i < total_pixels; i++) {
        int bin = get_bin_index(hist, data[i]);
        hist->bins[bin]++;
    }

    return hist;
}

PUBLIC histogram* chunk_histogram(const float* data,
                                      int dimz, int dimy, int dimx,
                                      int num_bins) {
    if (!data || num_bins <= 0) {
        return NULL;
    }

    float min_val = FLT_MAX;
    float max_val = -FLT_MAX;

    int total_voxels = dimz * dimy * dimx;
    for (int i = 0; i < total_voxels; i++) {
        float val = data[i];
        if (val < min_val) min_val = val;
        if (val > max_val) max_val = val;
    }

    histogram* hist = histogram_new(num_bins, min_val, max_val);
    if (!hist) {
        return NULL;
    }

    for (int i = 0; i < total_voxels; i++) {
        int bin = get_bin_index(hist, data[i]);
        hist->bins[bin]++;
    }

    return hist;
}

PRIVATE float get_slice_value(const float* data, int y, int x, int dimx) {
    return data[y * dimx + x];
}

PRIVATE float get_chunk_value(const float* data, int z, int y, int x,
                                  int dimy, int dimx) {
    return data[z * (dimy * dimx) + y * dimx + x];
}
PUBLIC int write_histogram_to_csv(const histogram *hist, const char *filename) {
  FILE *fp = fopen(filename, "w");
  if (!fp) {
    return 1;
  }

  fprintf(fp, "bin_start,bin_end,count\n");

  for (int i = 0; i < hist->num_bins; i++) {
    float bin_start = hist->min_value + i * hist->bin_width;
    float bin_end = bin_start + hist->bin_width;
    fprintf(fp, "%.6f,%.6f,%u\n", bin_start, bin_end, hist->bins[i]);
  }

  fclose(fp);
  return 0;
}

PUBLIC hist_stats calculate_histogram_stats(const histogram *hist) {
  hist_stats stats = {0};

  unsigned long long total_count = 0;
  double weighted_sum = 0.0;
  unsigned int max_count = 0;

  for (int i = 0; i < hist->num_bins; i++) {
    float bin_center = hist->min_value + (i + 0.5f) * hist->bin_width;
    weighted_sum += bin_center * hist->bins[i];
    total_count += hist->bins[i];

    if (hist->bins[i] > max_count) {
      max_count = hist->bins[i];
      stats.mode = bin_center;
      stats.mode_count = hist->bins[i];
    }
  }

  stats.mean = (float) (weighted_sum / total_count);

  double variance_sum = 0;
  for (int i = 0; i < hist->num_bins; i++) {
    float bin_center = hist->min_value + (i + 0.5f) * hist->bin_width;
    float diff = bin_center - stats.mean;
    variance_sum += diff * diff * hist->bins[i];
  }
  stats.std_dev = (float) sqrt(variance_sum / total_count);

  unsigned long long median_count = total_count / 2;
  unsigned long long running_count = 0;
  for (int i = 0; i < hist->num_bins; i++) {
    running_count += hist->bins[i];
    if (running_count >= median_count) {
      stats.median = hist->min_value + (i + 0.5f) * hist->bin_width;
      break;
    }
  }

  return stats;
}