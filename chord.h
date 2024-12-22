#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <math.h>

#define MAX_CHORDS 8192
#define MIN_CHORD_LENGTH 8
#define MAX_CHORD_LENGTH 128
#define NUM_DIMENSIONS 3

#include "snic.h"

// Basic chord structure - just stores list of superpixels
typedef struct {
    uint32_t* points;
    int point_count;
} Chord;

// Active chord used during growth
typedef struct {
    Chord chord;          // The underlying chord being built
    int front_idx;        // Current front superpixel index
    int back_idx;         // Current back superpixel index
    int axis;            // Growth axis
    float front_pos[3];  // Current front position
    float back_pos[3];   // Current back position
    float front_dir[3];  // Current front direction
    float back_dir[3];   // Current back direction
    int grow_direction;  // 1 for positive, -1 for negative
    bool is_active;      // Whether this chord is still growing
} ActiveChord;

// Helper functions for vector operations
static inline void vector_subtract(const float* v1, const float* v2, float* result) {
    for (int i = 0; i < NUM_DIMENSIONS; i++) {
        result[i] = v1[i] - v2[i];
    }
}

static inline float vector_magnitude(const float* v) {
    float sum = 0.0f;
    for (int i = 0; i < NUM_DIMENSIONS; i++) {
        sum += v[i] * v[i];
    }
    return sqrtf(sum);
}

static inline float vector_dot(const float* v1, const float* v2) {
    float sum = 0.0f;
    for (int i = 0; i < NUM_DIMENSIONS; i++) {
        sum += v1[i] * v2[i];
    }
    return sum;
}

static inline bool is_near_boundary(const float* pos, const float* min_bounds, const float* max_bounds) {
    for (int i = 0; i < NUM_DIMENSIONS; i++) {
        if (pos[i] < min_bounds[i] || pos[i] > max_bounds[i]) {
            return true;
        }
    }
    return false;
}

// Convert ActiveChord to basic Chord
static Chord active_chord_to_chord(const ActiveChord* active) {
    Chord chord;
    chord.point_count = active->chord.point_count;
    chord.points = malloc(chord.point_count * sizeof(uint32_t));
    memcpy(chord.points, active->chord.points, chord.point_count * sizeof(uint32_t));
    return chord;
}

// Find next superpixel in chord
bool find_best_next_superpixel(int current_idx,
                              const float* current_pos,
                              const float* current_dir,
                              int target_axis,
                              int direction,
                              const Superpixel* superpixels,
                              const SuperpixelConnections* connections,
                              bool* available,
                              const float* min_bounds,
                              const float* max_bounds,
                              int* best_idx,
                              float* best_direction) {

    float ideal_dir[NUM_DIMENSIONS] = {0};
    ideal_dir[target_axis] = (float)direction;

    float best_score = -INFINITY;
    *best_idx = -1;
    memset(best_direction, 0, NUM_DIMENSIONS * sizeof(float));

    const SuperpixelConnections* curr_connections = &connections[current_idx];
    const float curr_pos[3] = {superpixels[current_idx].x,
                              superpixels[current_idx].y,
                              superpixels[current_idx].z};

    // Check all connected superpixels
    for (int i = 0; i < curr_connections->num_connections; i++) {
        int next_idx = curr_connections->connections[i].neighbor_label;
        float strength = curr_connections->connections[i].connection_strength;

        if (!available[next_idx]) continue;

        const float next_pos[3] = {superpixels[next_idx].x,
                                  superpixels[next_idx].y,
                                  superpixels[next_idx].z};

        if (is_near_boundary(next_pos, min_bounds, max_bounds)) continue;

        float dp[NUM_DIMENSIONS];
        vector_subtract(next_pos, curr_pos, dp);
        float dist = vector_magnitude(dp);

        if (dist < 0.1f) continue;

        // Normalize direction vector
        for (int j = 0; j < NUM_DIMENSIONS; j++) {
            dp[j] /= dist;
        }

        float progress = dp[target_axis] * direction;
        if (progress < 0.3f) continue;  // Require significant progress

        float alignment_score = vector_dot(dp, ideal_dir);
        float score = alignment_score * strength;  // Weight by connection strength

        if (score > best_score) {
            best_score = score;
            *best_idx = next_idx;
            memcpy(best_direction, dp, NUM_DIMENSIONS * sizeof(float));
        }
    }

    return *best_idx >= 0;
}

// Grow single chord segment
bool grow_chord_segment(ActiveChord* active,
                      const Superpixel* superpixels,
                      const SuperpixelConnections* connections,
                      bool* available,
                      const float* min_bounds,
                      const float* max_bounds,
                      int max_length) {

    if (!active->is_active || active->chord.point_count >= max_length) {
        active->is_active = false;
        return false;
    }

    // Check overall progress
    if (active->chord.point_count > 1) {
        float first_pos[3] = {superpixels[active->chord.points[0]].x,
                             superpixels[active->chord.points[0]].y,
                             superpixels[active->chord.points[0]].z};
        float front_pos[3] = {superpixels[active->front_idx].x,
                             superpixels[active->front_idx].y,
                             superpixels[active->front_idx].z};
        float total_dp[NUM_DIMENSIONS];
        vector_subtract(front_pos, first_pos, total_dp);
        float total_dist = vector_magnitude(total_dp);

        if (total_dist > 0) {
            float overall_progress = (total_dp[active->axis] * active->grow_direction) / total_dist;
            if (overall_progress < 0.5f) {
                active->is_active = false;
                return false;
            }
        }
    }

    int next_idx;
    float next_dir[NUM_DIMENSIONS];

    if (!find_best_next_superpixel(active->front_idx,
                                  active->front_pos,
                                  active->front_dir,
                                  active->axis,
                                  active->grow_direction,
                                  superpixels,
                                  connections,
                                  available,
                                  min_bounds,
                                  max_bounds,
                                  &next_idx,
                                  next_dir)) {
        active->is_active = false;
        return false;
    }

    // Resize points array if needed
    if (active->chord.point_count >= max_length) {
        int new_size = fmin(max_length * 2, MAX_CHORD_LENGTH);
        uint32_t* new_points = realloc(active->chord.points, new_size * sizeof(uint32_t));
        if (!new_points) {
            active->is_active = false;
            return false;
        }
        active->chord.points = new_points;
    }

    // Add new point
    if (active->grow_direction > 0) {
        active->chord.points[active->chord.point_count] = next_idx;
    } else {
        memmove(&active->chord.points[1], &active->chord.points[0],
                active->chord.point_count * sizeof(uint32_t));
        active->chord.points[0] = next_idx;
    }

    active->chord.point_count++;
    available[next_idx] = false;

    // Update active chord state
    active->front_idx = next_idx;
    float next_pos[3] = {superpixels[next_idx].x,
                         superpixels[next_idx].y,
                         superpixels[next_idx].z};
    memcpy(active->front_pos, next_pos, NUM_DIMENSIONS * sizeof(float));
    memcpy(active->front_dir, next_dir, NUM_DIMENSIONS * sizeof(float));

    return true;
}

// Main chord growing function - now returns array of basic Chords
Chord* grow_chords(const Superpixel* superpixels,
                  const SuperpixelConnections* connections,
                  int num_superpixels,
                  float bounds[NUM_DIMENSIONS][2],
                  int axis,
                  int num_paths,
                  int* num_chords_out) {

    // Initialize working space
    bool* available = calloc(num_superpixels, sizeof(bool));
    memset(available, true, num_superpixels * sizeof(bool));

    float min_bounds[NUM_DIMENSIONS];
    float max_bounds[NUM_DIMENSIONS];
    for (int i = 0; i < NUM_DIMENSIONS; i++) {
        min_bounds[i] = bounds[i][0] + 0.1f;
        max_bounds[i] = bounds[i][1] - 0.2f;
    }

    // Initialize active chords
    int chords_per_direction = num_paths / 2;
    ActiveChord* active_chords = calloc(num_paths, sizeof(ActiveChord));
    int num_active_chords = 0;

    // Seed paths in both directions
    for (int dir = -1; dir <= 1; dir += 2) {
        for (int i = 0; i < chords_per_direction; i++) {
            // Find valid seed superpixel
            int seed_idx = rand() % num_superpixels;
            if (!available[seed_idx]) continue;

            ActiveChord* active = &active_chords[num_active_chords++];
            active->chord.points = malloc(100 * sizeof(uint32_t));
            active->chord.point_count = 1;
            active->front_idx = seed_idx;
            active->back_idx = seed_idx;
            active->axis = axis;
            active->grow_direction = dir;
            active->is_active = true;

            float seed_pos[3] = {superpixels[seed_idx].x,
                                superpixels[seed_idx].y,
                                superpixels[seed_idx].z};
            memcpy(active->front_pos, seed_pos, NUM_DIMENSIONS * sizeof(float));
            memcpy(active->back_pos, seed_pos, NUM_DIMENSIONS * sizeof(float));

            float ideal_dir[NUM_DIMENSIONS] = {0};
            ideal_dir[axis] = dir;
            memcpy(active->front_dir, ideal_dir, NUM_DIMENSIONS * sizeof(float));
            memcpy(active->back_dir, ideal_dir, NUM_DIMENSIONS * sizeof(float));

            active->chord.points[0] = seed_idx;
            available[seed_idx] = false;
        }
    }

    // Growth phase
    bool any_growth;
    do {
        any_growth = false;
        for (int i = 0; i < num_active_chords; i++) {
            if (active_chords[i].is_active) {
                any_growth |= grow_chord_segment(&active_chords[i],
                                              superpixels,
                                              connections,
                                              available,
                                              min_bounds,
                                              max_bounds,
                                              MAX_CHORD_LENGTH);
            }
        }
    } while (any_growth);

    // Count valid chords
    int valid_chord_count = 0;
    for (int i = 0; i < num_active_chords; i++) {
        if (active_chords[i].chord.point_count >= MIN_CHORD_LENGTH) {
            valid_chord_count++;
        }
    }

    // Create output array of basic Chords
    Chord* result_chords = calloc(valid_chord_count, sizeof(Chord));
    int result_idx = 0;

    for (int i = 0; i < num_active_chords; i++) {
        if (active_chords[i].chord.point_count >= MIN_CHORD_LENGTH) {
            result_chords[result_idx] = active_chord_to_chord(&active_chords[i]);
            result_idx++;
        }
    }

    // Cleanup temporary arrays but not the result
    for (int i = 0; i < num_active_chords; i++) {
        free(active_chords[i].chord.points);
    }
    free(active_chords);
    free(available);
    
    *num_chords_out = valid_chord_count;
    return result_chords;
}