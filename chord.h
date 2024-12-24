#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <math.h>

#define MAX_CHORDS 8192
#define MIN_CHORD_LENGTH 16
#define MAX_CHORD_LENGTH 128
#define NUM_DIMENSIONS 3
#define MAX_RECENT_DIRS 3
#define NUM_LAYERS 32
#define SMOOTHNESS_THRESHOLD 0.8f
#define PROGRESS_THRESHOLD 0.5f
#define MIN_CONNECTIONS 1
#define KD_TREE_K 16
#define KD_TREE_MAX_DIST 16.0f
#define MAX_RECORDS_PER_CELL 64

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
typedef struct Chord {
    uint32_t* points;       // Array of superpixel indices
    int point_count;
    float* recent_dirs;     // Keep this for direction tracking
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
static int* select_start_points(const Superpixel* superpixels,
                              const SuperpixelConnections* connections,
                              int num_superpixels,
                              float bounds[NUM_DIMENSIONS][2],
                              int target_count,
                              int axis,
                              int* num_starts) {
    if (!superpixels || !connections || !bounds || !num_starts || num_superpixels <= 0) {
        return NULL;
    }

    float axis_min = bounds[axis][0];
    float axis_max = bounds[axis][1];
    float axis_step = (axis_max - axis_min) / NUM_LAYERS;
    int points_per_layer = target_count / NUM_LAYERS;

    // Pre-allocate all arrays
    int* starts = malloc(target_count * sizeof(int));
    float* intensities = malloc(num_superpixels * sizeof(float));
    int* all_layer_points = malloc(num_superpixels * NUM_LAYERS * sizeof(int));

    if (!starts || !intensities || !all_layer_points) {
        free(starts);
        free(intensities);
        free(all_layer_points);
        return NULL;
    }

    *num_starts = 0;

    // Calculate intensity threshold
    for (int i = 0; i < num_superpixels; i++) {
        intensities[i] = superpixels[i].c;
    }
    float min_intensity = calculate_percentile(intensities, num_superpixels, 75.0f);
    free(intensities);

    int total_layer_points = 0;
    for (int layer = 0; layer < NUM_LAYERS && *num_starts < target_count; layer++) {
        float layer_min = axis_min + layer * axis_step;
        float layer_max = layer_min + axis_step;
        int layer_start_idx = total_layer_points;
        int num_layer_points = 0;

        for (int i = 0; i < num_superpixels; i++) {
            if (i >= num_superpixels) break;

            float pos = (axis == 0) ? superpixels[i].z :
                       (axis == 1) ? superpixels[i].y :
                                   superpixels[i].x;

            if (pos >= layer_min && pos < layer_max &&
                superpixels[i].c > min_intensity &&
                connections[i].connections &&
                connections[i].num_connections >= MIN_CONNECTIONS) {
                all_layer_points[total_layer_points + num_layer_points++] = i;
            }
        }

        if (num_layer_points > 0) {
            int remaining_slots = target_count - *num_starts;
            int to_select = (points_per_layer < num_layer_points) ?
                           points_per_layer : num_layer_points;
            to_select = (to_select < remaining_slots) ?
                       to_select : remaining_slots;

            for (int i = 0; i < to_select && num_layer_points > 0; i++) {
                int idx = rand() % num_layer_points;
                starts[(*num_starts)++] = all_layer_points[layer_start_idx + idx];
                all_layer_points[layer_start_idx + idx] =
                    all_layer_points[layer_start_idx + --num_layer_points];
            }
        }
        total_layer_points += num_layer_points;
    }

    free(all_layer_points);
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
                             int num_superpixels) {
    // Validate start point
    if (!is_valid_superpixel(start_point, num_superpixels)) {
        Chord empty = {0};
        empty.points = malloc(sizeof(uint32_t));
        empty.recent_dirs = malloc(MAX_RECENT_DIRS * NUM_DIMENSIONS * sizeof(float));
        empty.point_count = 0;
        return empty;
    }

    Chord chord = {0};
    chord.points = malloc(MAX_CHORD_LENGTH * sizeof(uint32_t));
    chord.recent_dirs = malloc(MAX_RECENT_DIRS * NUM_DIMENSIONS * sizeof(float));
    chord.num_recent_dirs = 0;

    // Add initial point
    chord.points[0] = start_point;
    chord.point_count = 1;
    available[start_point] = false;

    // Create buffer for temporary growth in each direction
    uint32_t* temp_points = malloc(MAX_CHORD_LENGTH * sizeof(uint32_t));
    int temp_count = 0;

    // Grow in both directions
    for (int direction = -1; direction <= 1; direction += 2) {
        temp_count = 0;  // Always start with empty temp buffer

        int current = start_point;  // Always start from the initial point
        const Superpixel* current_sp = &superpixels[current];
        float current_pos[3] = {current_sp->z, current_sp->y, current_sp->x};

        while (temp_count < MAX_CHORD_LENGTH) {
            float best_score = -INFINITY;
            int best_next = -1;
            float best_dir[NUM_DIMENSIONS];
            const Superpixel* best_next_sp = NULL;

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
                const Superpixel* next_sp = &superpixels[next];

                float next_pos[3] = {next_sp->z, next_sp->y, next_sp->x};
                float dp[NUM_DIMENSIONS];
                vector_subtract(next_pos, current_pos, dp);
                float dist = vector_magnitude(dp);

                if (dist < 0.01f) continue;

                // Normalize direction
                for (int j = 0; j < NUM_DIMENSIONS; j++) {
                    dp[j] /= dist;
                }

                // Calculate axis progress with direction
                float axis_progress = direction * dp[axis];
                if (axis_progress < PROGRESS_THRESHOLD * 0.5f) continue;

                // Calculate smoothness
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

                // Calculate alignment
                float connection_alignment = fabsf(vector_dot(dp, strong_dir));
                if (isnan(connection_alignment)) connection_alignment = 0.5f;

                // Get parallel score
                float parallel_score = get_parallel_score(tracker, next_pos, dp);

                // Calculate final score
                float total_score =
                    (strength / 255.0f) * 0.1f +
                    axis_progress * 0.7f +
                    parallel_score * 0.1f +
                    connection_alignment * 0.1f;

                if (total_score > best_score) {
                    best_score = total_score;
                    best_next = next;
                    memcpy(best_dir, dp, NUM_DIMENSIONS * sizeof(float));
                    best_next_sp = next_sp;
                }
            }

            if (best_next < 0 || !is_valid_superpixel(best_next, num_superpixels)) break;

            // Add point to temp buffer
            temp_points[temp_count++] = best_next;

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
            float best_next_pos[3] = {best_next_sp->z, best_next_sp->y, best_next_sp->x};
            tracker_add_direction(tracker, best_next_pos, best_dir);

            current = best_next;
            current_sp = best_next_sp;
            memcpy(current_pos, best_next_pos, NUM_DIMENSIONS * sizeof(float));
        }

        // Merge temp points into chord
        if (direction > 0) {
            // Forward direction: append to end
            if (chord.point_count + temp_count <= MAX_CHORD_LENGTH) {
                memcpy(&chord.points[chord.point_count],
                       temp_points,
                       temp_count * sizeof(uint32_t));
                chord.point_count += temp_count;
            }
        } else {
            // Backward direction: prepend to beginning
            if (chord.point_count + temp_count <= MAX_CHORD_LENGTH) {
                // First move existing points to make room
                memmove(&chord.points[temp_count],
                       chord.points,
                       chord.point_count * sizeof(uint32_t));

                // Then copy temp points at the beginning in reverse order
                for (int i = 0; i < temp_count; i++) {
                    chord.points[i] = temp_points[temp_count - 1 - i];
                }
                chord.point_count += temp_count;
            }
        }
    }

    free(temp_points);
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
        free(chords[i].recent_dirs);
    }
    free(chords);
}

typedef struct ChordStats {
    // Length stats
    int num_superpixels;
    float total_path_length;
    float avg_step_distance;
    float straightness;

    // Intensity stats
    float avg_intensity;
    float min_intensity;
    float max_intensity;
    float intensity_stddev;

    // Spatial stats
    float bbox[NUM_DIMENSIONS][2];  // min/max for each dimension
    float center_of_mass[NUM_DIMENSIONS];
    float avg_axis_deviation;

    // Connection stats
    float avg_connection_strength;
    int min_connections;
    int max_connections;
} ChordStats;

// Also update the chord stats calculation to use superpixel coordinates
ChordStats* analyze_chords(const Chord* chords,
                          int num_chords,
                          const Superpixel* superpixels,
                          const SuperpixelConnections* connections) {
    ChordStats* stats = malloc(num_chords * sizeof(ChordStats));

    for (int i = 0; i < num_chords; i++) {
        const Chord* chord = &chords[i];
        ChordStats* chord_stats = &stats[i];
        memset(chord_stats, 0, sizeof(ChordStats));

        // Initialize bbox with first point's coordinates
        const Superpixel* first_sp = &superpixels[chord->points[0]];
        chord_stats->bbox[0][0] = chord_stats->bbox[0][1] = first_sp->z;
        chord_stats->bbox[1][0] = chord_stats->bbox[1][1] = first_sp->y;
        chord_stats->bbox[2][0] = chord_stats->bbox[2][1] = first_sp->x;

        // Basic length stat
        chord_stats->num_superpixels = chord->point_count;

        float total_intensity = 0;
        chord_stats->min_intensity = INFINITY;
        chord_stats->max_intensity = -INFINITY;

        // First pass to collect basic stats and bbox
        for (int j = 0; j < chord->point_count; j++) {
            const Superpixel* sp = &superpixels[chord->points[j]];

            // Intensity stats
            total_intensity += sp->c;
            chord_stats->min_intensity = fminf(chord_stats->min_intensity, sp->c);
            chord_stats->max_intensity = fmaxf(chord_stats->max_intensity, sp->c);

            // Update bbox and center of mass
            chord_stats->bbox[0][0] = fminf(chord_stats->bbox[0][0], sp->z);
            chord_stats->bbox[0][1] = fmaxf(chord_stats->bbox[0][1], sp->z);
            chord_stats->bbox[1][0] = fminf(chord_stats->bbox[1][0], sp->y);
            chord_stats->bbox[1][1] = fmaxf(chord_stats->bbox[1][1], sp->y);
            chord_stats->bbox[2][0] = fminf(chord_stats->bbox[2][0], sp->x);
            chord_stats->bbox[2][1] = fmaxf(chord_stats->bbox[2][1], sp->x);

            chord_stats->center_of_mass[0] += sp->z;
            chord_stats->center_of_mass[1] += sp->y;
            chord_stats->center_of_mass[2] += sp->x;

            // Connection stats
            int num_connections = connections[chord->points[j]].num_connections;
            chord_stats->min_connections = j == 0 ? num_connections :
                                         MIN(chord_stats->min_connections, num_connections);
            chord_stats->max_connections = MAX(chord_stats->max_connections, num_connections);

            float total_strength = 0;
            for (int k = 0; k < num_connections; k++) {
                total_strength += connections[chord->points[j]].connections[k].connection_strength;
            }
            chord_stats->avg_connection_strength += total_strength / num_connections;
        }

        // Calculate averages
        chord_stats->avg_intensity = total_intensity / chord->point_count;
        chord_stats->avg_connection_strength /= chord->point_count;

        // Finalize center of mass
        for (int d = 0; d < NUM_DIMENSIONS; d++) {
            chord_stats->center_of_mass[d] /= chord->point_count;
        }

        // Calculate path length and straightness
        chord_stats->total_path_length = 0;
        for (int j = 1; j < chord->point_count; j++) {
            const Superpixel* curr = &superpixels[chord->points[j]];
            const Superpixel* prev = &superpixels[chord->points[j-1]];

            float step_dist = 0;
            step_dist += (curr->z - prev->z) * (curr->z - prev->z);
            step_dist += (curr->y - prev->y) * (curr->y - prev->y);
            step_dist += (curr->x - prev->x) * (curr->x - prev->x);
            step_dist = sqrtf(step_dist);

            chord_stats->total_path_length += step_dist;
        }
        chord_stats->avg_step_distance = chord_stats->total_path_length / (chord->point_count - 1);

        // Calculate end-to-end distance for straightness
        const Superpixel* first = &superpixels[chord->points[0]];
        const Superpixel* last = &superpixels[chord->points[chord->point_count-1]];
        float end_to_end = 0;
        end_to_end += (last->z - first->z) * (last->z - first->z);
        end_to_end += (last->y - first->y) * (last->y - first->y);
        end_to_end += (last->x - first->x) * (last->x - first->x);
        end_to_end = sqrtf(end_to_end);

        chord_stats->straightness = end_to_end / chord_stats->total_path_length;
    }

    return stats;
}

void write_chord_stats_csv(const char* path, const ChordStats* stats, int num_chords) {
    FILE* fp = fopen(path, "w");
    if (!fp) return;

    fprintf(fp, "chord_id,num_superpixels,total_length,avg_step,straightness,avg_intensity,"
            "min_intensity,max_intensity,bbox_z_size,bbox_y_size,bbox_x_size\n");

    for (int i = 0; i < num_chords; i++) {
        const ChordStats* s = &stats[i];
        fprintf(fp, "%d,%d,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f\n",
                i, s->num_superpixels, s->total_path_length, s->avg_step_distance,
                s->straightness, s->avg_intensity, s->min_intensity, s->max_intensity,
                s->bbox[0][1] - s->bbox[0][0],  // x size
                s->bbox[1][1] - s->bbox[1][0],  // y size
                s->bbox[2][1] - s->bbox[2][0]); // z size
    }

    fclose(fp);
}