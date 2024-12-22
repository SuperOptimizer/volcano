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
#define MAX_RECENT_DIRS 3
#define NUM_LAYERS 256
#define SMOOTHNESS_THRESHOLD 0.8f
#define PROGRESS_THRESHOLD 0.5f
#define MIN_CONNECTIONS 4
#define KD_TREE_K 8
#define KD_TREE_MAX_DIST 8.0f
#define MAX_RECORDS_PER_CELL 32

#include "snic.h"

// Direction record for the volume tracker
typedef struct DirectionRecord {
    float pos[NUM_DIMENSIONS];
    float dir[NUM_DIMENSIONS];
    struct DirectionRecord* next;
} DirectionRecord;

// Cell in spatial grid
typedef struct {
    DirectionRecord* records;
    int count;
} SpatialCell;

// Volume tracker to maintain parallelism between chords
typedef struct {
    DirectionRecord* records;
    int num_records;
    int capacity;
    // Spatial grid
    SpatialCell* cells;
    int cells_per_dim;
    float cell_size[NUM_DIMENSIONS];
    float min_bounds[NUM_DIMENSIONS];
} VolumeTracker;

// Enhanced chord structure
typedef struct {
    uint32_t* points;
    int point_count;
    float* positions;
    float* recent_dirs;
    int num_recent_dirs;
} Chord;


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

// Quick sort implementation for percentile calculation
static int partition(float* arr, int low, int high) {
    float pivot = arr[high];
    int i = (low - 1);

    for (int j = low; j <= high - 1; j++) {
        if (arr[j] <= pivot) {
            i++;
            float temp = arr[i];
            arr[i] = arr[j];
            arr[j] = temp;
        }
    }
    float temp = arr[i + 1];
    arr[i + 1] = arr[high];
    arr[high] = temp;
    return (i + 1);
}

// Iterative quicksort implementation to prevent stack overflow
static void quicksort(float* arr, int low, int high) {
    // Create an auxiliary stack
    int* stack = malloc(sizeof(int) * (high - low + 1));
    int top = -1;

    // Push initial values of low and high to stack
    stack[++top] = low;
    stack[++top] = high;

    // Keep popping from stack while is not empty
    while (top >= 0) {
        // Pop high and low
        high = stack[top--];
        low = stack[top--];

        // Get pivot element
        int pivot = partition(arr, low, high);

        // If there are elements on left side of pivot,
        // then push left side to stack
        if (pivot - 1 > low) {
            stack[++top] = low;
            stack[++top] = pivot - 1;
        }

        // If there are elements on right side of pivot,
        // then push right side to stack
        if (pivot + 1 < high) {
            stack[++top] = pivot + 1;
            stack[++top] = high;
        }
    }

    free(stack);
}

// Alternative approach using median-of-three partitioning if stack space is still an issue
static float median_of_three(float* arr, int low, int high) {
    int mid = low + (high - low) / 2;

    // Sort low, mid, high values
    if (arr[low] > arr[mid]) {
        float temp = arr[low];
        arr[low] = arr[mid];
        arr[mid] = temp;
    }
    if (arr[mid] > arr[high]) {
        float temp = arr[mid];
        arr[mid] = arr[high];
        arr[high] = temp;
        if (arr[low] > arr[mid]) {
            temp = arr[low];
            arr[low] = arr[mid];
            arr[mid] = temp;
        }
    }

    // Put median at high-1
    float temp = arr[mid];
    arr[mid] = arr[high-1];
    arr[high-1] = temp;
    return arr[high-1];
}

// Calculate percentile value from array
static float calculate_percentile(float* arr, int n, float percentile) {
    quicksort(arr, 0, n - 1);
    int index = (int)(percentile * n / 100.0f);
    return arr[index];
}

// Get cell index for position
static void get_cell_indices(const VolumeTracker* tracker, const float* pos, int* indices) {
    for (int i = 0; i < NUM_DIMENSIONS; i++) {
        float relative_pos = pos[i] - tracker->min_bounds[i];
        indices[i] = (int)(relative_pos / tracker->cell_size[i]);
        if (indices[i] < 0) indices[i] = 0;
        if (indices[i] >= tracker->cells_per_dim) indices[i] = tracker->cells_per_dim - 1;
    }
}

static int get_cell_index(const VolumeTracker* tracker, const int* indices) {
    return indices[0] +
           indices[1] * tracker->cells_per_dim +
           indices[2] * tracker->cells_per_dim * tracker->cells_per_dim;
}


// Get the strongest connection direction
static void get_strongest_connection_dir(const SuperpixelConnections* connections,
                                       int current,
                                       const Superpixel* superpixels,
                                       float* strong_dir) {
    float max_strength = 0.0f;
    float best_dir[NUM_DIMENSIONS] = {0};

    for (int i = 0; i < connections[current].num_connections; i++) {
        float strength = connections[current].connections[i].connection_strength;
        if (strength > max_strength) {
            int neighbor = connections[current].connections[i].neighbor_label;
            float dp[NUM_DIMENSIONS] = {
                superpixels[neighbor].z - superpixels[current].z,
                superpixels[neighbor].y - superpixels[current].y,
                superpixels[neighbor].x - superpixels[current].x
            };
            float mag = vector_magnitude(dp);
            if (mag > 0.001f) {
                max_strength = strength;
                for (int j = 0; j < NUM_DIMENSIONS; j++) {
                    best_dir[j] = dp[j] / mag;
                }
            }
        }
    }

    memcpy(strong_dir, best_dir, NUM_DIMENSIONS * sizeof(float));
}

// Initialize volume tracker
static VolumeTracker* create_volume_tracker(float bounds[NUM_DIMENSIONS][2]) {
    VolumeTracker* tracker = malloc(sizeof(VolumeTracker));

    // Initialize record storage
    tracker->records = malloc(1024 * sizeof(DirectionRecord));
    tracker->capacity = 1024;
    tracker->num_records = 0;

    // Initialize spatial grid
    tracker->cells_per_dim = 32;
    int total_cells = tracker->cells_per_dim * tracker->cells_per_dim * tracker->cells_per_dim;
    tracker->cells = calloc(total_cells, sizeof(SpatialCell));

    // Calculate cell sizes and bounds
    for (int i = 0; i < NUM_DIMENSIONS; i++) {
        tracker->min_bounds[i] = bounds[i][0];
        tracker->cell_size[i] = (bounds[i][1] - bounds[i][0]) / tracker->cells_per_dim;
    }

    return tracker;
}

// Add direction to tracker
static void tracker_add_direction(VolumeTracker* tracker, const float* pos, const float* dir) {
    // Expand storage if needed
    if (tracker->num_records >= tracker->capacity) {
        tracker->capacity *= 2;
        tracker->records = realloc(tracker->records, tracker->capacity * sizeof(DirectionRecord));
    }

    // Add to records
    DirectionRecord* record = &tracker->records[tracker->num_records++];
    memcpy(record->pos, pos, NUM_DIMENSIONS * sizeof(float));
    memcpy(record->dir, dir, NUM_DIMENSIONS * sizeof(float));

    // Add to spatial grid
    int indices[NUM_DIMENSIONS];
    get_cell_indices(tracker, pos, indices);
    int cell_idx = get_cell_index(tracker, indices);

    // Add to cell if not full
    SpatialCell* cell = &tracker->cells[cell_idx];
    if (cell->count < MAX_RECORDS_PER_CELL) {
        record->next = cell->records;
        cell->records = record;
        cell->count++;
    }
}

// Get parallel score for proposed direction
// Get parallel score for proposed direction
static float get_parallel_score(VolumeTracker* tracker, const float* pos, const float* proposed_dir) {
    if (tracker->num_records == 0) return 1.0f;

    // Find cell and neighboring cells
    int center_indices[NUM_DIMENSIONS];
    get_cell_indices(tracker, pos, center_indices);

    float total_alignment = 0.0f;
    int count = 0;

    // Check 3x3x3 neighborhood of cells
    for (int dz = -1; dz <= 1 && count < KD_TREE_K; dz++) {
        for (int dy = -1; dy <= 1 && count < KD_TREE_K; dy++) {
            for (int dx = -1; dx <= 1 && count < KD_TREE_K; dx++) {
                int indices[NUM_DIMENSIONS] = {
                    center_indices[0] + dx,
                    center_indices[1] + dy,
                    center_indices[2] + dz
                };

                // Skip if outside grid
                if (indices[0] < 0 || indices[0] >= tracker->cells_per_dim ||
                    indices[1] < 0 || indices[1] >= tracker->cells_per_dim ||
                    indices[2] < 0 || indices[2] >= tracker->cells_per_dim)
                    continue;

                int cell_idx = get_cell_index(tracker, indices);

                // Verify cell index
                if (cell_idx < 0 || cell_idx >= tracker->cells_per_dim * tracker->cells_per_dim * tracker->cells_per_dim)
                    continue;

                // Safety check the cell itself
                if (!tracker->cells[cell_idx].records)
                    continue;

                DirectionRecord* record = tracker->cells[cell_idx].records;

                // Check records in this cell
                while (record && count < KD_TREE_K) {
                    // Verify record is within our allocated records array
                    if (record < tracker->records ||
                        record >= tracker->records + tracker->num_records)
                        break;

                    float dp[NUM_DIMENSIONS] = {0};  // Initialize to zero
                    vector_subtract(record->pos, pos, dp);
                    float dist = vector_magnitude(dp);

                    if (dist <= KD_TREE_MAX_DIST) {
                        float alignment = fabsf(vector_dot(proposed_dir, record->dir));
                        total_alignment += alignment;
                        count++;
                    }
                    record = record->next;
                }
            }
        }
    }

    return count > 0 ? total_alignment / count : 1.0f;
}

// Select start points for chord growth
static int* select_start_points(const Superpixel* superpixels,
                              const SuperpixelConnections* connections,
                              int num_superpixels,
                              float bounds[NUM_DIMENSIONS][2],
                              int target_count,
                              int axis,
                              int* num_starts) {
    float axis_min = bounds[axis][0];
    float axis_max = bounds[axis][1];
    float axis_step = (axis_max - axis_min) / NUM_LAYERS;
    int points_per_layer = target_count / NUM_LAYERS;

    // Allocate maximum possible size
    int* starts = malloc(target_count * sizeof(int));
    *num_starts = 0;

    // Calculate intensity threshold (5th percentile)
    float* intensities = malloc(num_superpixels * sizeof(float));
    for (int i = 0; i < num_superpixels; i++) {
        intensities[i] = superpixels[i].c;
    }
    float min_intensity = calculate_percentile(intensities, num_superpixels, 5.0f);
    free(intensities);

    // Select points for each layer
    for (int layer = 0; layer < NUM_LAYERS; layer++) {
        float layer_min = axis_min + layer * axis_step;
        float layer_max = layer_min + axis_step;

        // Find valid points in this layer
        int* layer_points = malloc(num_superpixels * sizeof(int));
        int num_layer_points = 0;

        for (int i = 1; i <= num_superpixels; i++) {
            float pos = axis == 0 ? superpixels[i].z :
                       axis == 1 ? superpixels[i].y :
                                 superpixels[i].x;

            if (pos >= layer_min && pos < layer_max &&
                superpixels[i].c > min_intensity &&
                connections[i].num_connections >= MIN_CONNECTIONS) {
                layer_points[num_layer_points++] = i;
            }
        }

        // Randomly select points from this layer
        if (num_layer_points > 0) {
            int to_select = points_per_layer < num_layer_points ?
                           points_per_layer : num_layer_points;

            for (int i = 0; i < to_select; i++) {
                int idx = rand() % num_layer_points;
                starts[(*num_starts)++] = layer_points[idx];
                // Swap with last element and reduce size
                layer_points[idx] = layer_points[--num_layer_points];
            }
        }

        free(layer_points);
    }

    return starts;
}

// Add validation for superpixel indices
static bool is_valid_superpixel(int index, int max_index) {
    return index > 0 && index <= max_index;  // 1-based indexing validation
}

static Chord grow_single_chord(int start_point,
                             const Superpixel* superpixels,
                             const SuperpixelConnections* connections,
                             bool* available,
                             VolumeTracker* tracker,
                             float bounds[NUM_DIMENSIONS][2],
                             int axis,
                             int num_superpixels) {  // Add num_superpixels parameter for validation
    // Validate start point
    if (!is_valid_superpixel(start_point, num_superpixels)) {
        Chord empty = {0};
        empty.points = malloc(sizeof(uint32_t));
        empty.positions = malloc(NUM_DIMENSIONS * sizeof(float));
        empty.recent_dirs = malloc(MAX_RECENT_DIRS * NUM_DIMENSIONS * sizeof(float));
        empty.point_count = 0;
        return empty;
    }

    Chord chord = {0};
    chord.points = malloc(MAX_CHORD_LENGTH * sizeof(uint32_t));
    chord.positions = malloc(MAX_CHORD_LENGTH * NUM_DIMENSIONS * sizeof(float));
    chord.recent_dirs = malloc(MAX_RECENT_DIRS * NUM_DIMENSIONS * sizeof(float));
    chord.num_recent_dirs = 0;

    // Add initial point
    chord.points[0] = start_point;
    float* start_pos = &chord.positions[0];
    start_pos[0] = superpixels[start_point].z;
    start_pos[1] = superpixels[start_point].y;
    start_pos[2] = superpixels[start_point].x;
    chord.point_count = 1;
    available[start_point] = false;

    // Create buffers for temporary growth in each direction
    uint32_t* temp_points = malloc(MAX_CHORD_LENGTH * sizeof(uint32_t));
    float* temp_positions = malloc(MAX_CHORD_LENGTH * NUM_DIMENSIONS * sizeof(float));
    int temp_count = 0;

    // Grow in both directions
    for (int direction = -1; direction <= 1; direction += 2) {
        if (direction > 0) {
            temp_count = 0;
        } else {
            for (int i = 0; i < chord.point_count; i++) {
                temp_points[i] = chord.points[chord.point_count - 1 - i];
                memcpy(&temp_positions[i * NUM_DIMENSIONS],
                       &chord.positions[(chord.point_count - 1 - i) * NUM_DIMENSIONS],
                       NUM_DIMENSIONS * sizeof(float));
            }
            temp_count = chord.point_count;
        }

        int current = direction > 0 ? chord.points[chord.point_count - 1] : start_point;
        if (!is_valid_superpixel(current, num_superpixels)) break;

        float current_pos[3] = {superpixels[current].z,
                               superpixels[current].y,
                               superpixels[current].x};

        while (temp_count < MAX_CHORD_LENGTH) {
            float best_score = -INFINITY;
            int best_next = -1;
            float best_dir[NUM_DIMENSIONS];
            float best_next_pos[NUM_DIMENSIONS];

            // Get strongest connection direction
            float strong_dir[NUM_DIMENSIONS];
            get_strongest_connection_dir(connections, current, superpixels, strong_dir);

            // Check all connections
            for (int i = 0; i < connections[current].num_connections; i++) {
                int next = connections[current].connections[i].neighbor_label;

                // Validate next superpixel index
                if (!is_valid_superpixel(next, num_superpixels) || !available[next])
                    continue;

                float strength = connections[current].connections[i].connection_strength;

                float next_pos[3] = {superpixels[next].z,
                                   superpixels[next].y,
                                   superpixels[next].x};

                float dp[NUM_DIMENSIONS];
                vector_subtract(next_pos, current_pos, dp);
                float dist = vector_magnitude(dp);

                if (dist < 0.01f) continue;

                // Normalize direction
                for (int j = 0; j < NUM_DIMENSIONS; j++) {
                    dp[j] /= dist;
                }

                // Calculate axis progress - reduced threshold for longer chords
                float axis_progress = direction * dp[axis];
                if (axis_progress < PROGRESS_THRESHOLD * 0.5f) continue;

                // Calculate smoothness with more permissive threshold
                float smoothness_score = 1.0f;
                if (chord.num_recent_dirs > 0) {
                    float total_smooth = 0.0f;
                    for (int j = 0; j < chord.num_recent_dirs; j++) {
                        total_smooth += vector_dot(dp,
                            &chord.recent_dirs[j * NUM_DIMENSIONS]);
                    }
                    smoothness_score = total_smooth / chord.num_recent_dirs;
                    if (smoothness_score < SMOOTHNESS_THRESHOLD * 0.7f) continue;
                }

                // Calculate alignment with strongest connection direction
                float connection_alignment = fabsf(vector_dot(dp, strong_dir));
                if (isnan(connection_alignment)) connection_alignment = 0.5f;

                // Get parallel score from volume tracker
                float parallel_score = get_parallel_score(tracker, next_pos, dp);

                // Calculate final score with adjusted weights to prioritize connection strength
                float total_score =
                    (strength / 255.0f) * 0.6f +        // Increased weight for connection strength
                    axis_progress * 0.2f +              // Moderate axis bias
                    parallel_score * 0.1f +             // Light parallel influence
                    connection_alignment * 0.1f;        // Reduced alignment influence

                if (total_score > best_score) {
                    best_score = total_score;
                    best_next = next;
                    memcpy(best_dir, dp, NUM_DIMENSIONS * sizeof(float));
                    memcpy(best_next_pos, next_pos, NUM_DIMENSIONS * sizeof(float));
                }
            }

            if (best_next < 0 || !is_valid_superpixel(best_next, num_superpixels)) break;

            // Add point to temp buffers
            temp_points[temp_count] = best_next;
            memcpy(&temp_positions[temp_count * NUM_DIMENSIONS],
                   best_next_pos,
                   NUM_DIMENSIONS * sizeof(float));
            temp_count++;

            // Update recent directions
            if (chord.num_recent_dirs < MAX_RECENT_DIRS) {
                memcpy(&chord.recent_dirs[chord.num_recent_dirs * NUM_DIMENSIONS],
                       best_dir, NUM_DIMENSIONS * sizeof(float));
                chord.num_recent_dirs++;
            } else {
                memmove(chord.recent_dirs,
                        &chord.recent_dirs[NUM_DIMENSIONS],
                        (MAX_RECENT_DIRS - 1) * NUM_DIMENSIONS * sizeof(float));
                memcpy(&chord.recent_dirs[(MAX_RECENT_DIRS - 1) * NUM_DIMENSIONS],
                       best_dir, NUM_DIMENSIONS * sizeof(float));
            }

            available[best_next] = false;
            tracker_add_direction(tracker, best_next_pos, best_dir);

            current = best_next;
            memcpy(current_pos, best_next_pos, NUM_DIMENSIONS * sizeof(float));
        }

        // Merge temp buffers into chord
        if (direction > 0) {
            if (chord.point_count + temp_count <= MAX_CHORD_LENGTH) {
                memcpy(&chord.points[chord.point_count],
                       temp_points,
                       temp_count * sizeof(uint32_t));
                memcpy(&chord.positions[chord.point_count * NUM_DIMENSIONS],
                       temp_positions,
                       temp_count * NUM_DIMENSIONS * sizeof(float));
                chord.point_count += temp_count;
            }
        } else {
            if (chord.point_count + temp_count <= MAX_CHORD_LENGTH) {
                memmove(&chord.points[temp_count],
                       chord.points,
                       chord.point_count * sizeof(uint32_t));
                memmove(&chord.positions[temp_count * NUM_DIMENSIONS],
                       chord.positions,
                       chord.point_count * NUM_DIMENSIONS * sizeof(float));

                memcpy(chord.points,
                       temp_points,
                       temp_count * sizeof(uint32_t));
                memcpy(chord.positions,
                       temp_positions,
                       temp_count * NUM_DIMENSIONS * sizeof(float));
                chord.point_count += temp_count;
            }
        }
    }

    free(temp_points);
    free(temp_positions);
    return chord;
}

// Main chord growing function
Chord* grow_chords(const Superpixel* superpixels,
                  const SuperpixelConnections* connections,
                  int num_superpixels,
                  float bounds[NUM_DIMENSIONS][2],
                  int axis,
                  int num_paths,
                  int* num_chords_out) {
    // Initialize working space
    bool* available = calloc(num_superpixels + 1, sizeof(bool));
    memset(available, true, (num_superpixels + 1) * sizeof(bool));

    VolumeTracker* tracker = create_volume_tracker(bounds);

    // Select start points
    int num_starts;
    int* start_points = select_start_points(superpixels, connections,
                                          num_superpixels, bounds,
                                          num_paths, axis, &num_starts);

    // Grow chords from each start point
    Chord* chords = malloc(num_starts * sizeof(Chord));
    int valid_chords = 0;

    for (int i = 0; i < num_starts; i++) {
        if (!available[start_points[i]]) continue;

        Chord chord = grow_single_chord(start_points[i], superpixels,
                                      connections, available, tracker,
                                      bounds, axis, num_superpixels);

        if (chord.point_count >= MIN_CHORD_LENGTH) {
            chords[valid_chords++] = chord;
        } else {
            free(chord.points);
            free(chord.positions);
            free(chord.recent_dirs);
        }
    }

    // Create final chord array of exact size needed
    Chord* final_chords = malloc(valid_chords * sizeof(Chord));
    memcpy(final_chords, chords, valid_chords * sizeof(Chord));

    // Cleanup
    free(chords);
    free(start_points);
    free(available);
    free(tracker->records);
    free(tracker->cells);
    free(tracker);

    *num_chords_out = valid_chords;
    return final_chords;
}

// Helper function to free chord memory
void free_chords(Chord* chords, int num_chords) {
    for (int i = 0; i < num_chords; i++) {
        free(chords[i].points);
        free(chords[i].positions);
        free(chords[i].recent_dirs);
    }
    free(chords);
}