#pragma once

#include "vesuvius-c.h"

typedef struct queue_node {
    int z, y, x;
    struct queue_node* next;
} queue_node;

typedef struct queue {
    queue_node *front, *rear;
} queue;

// Queue operations
static inline queue* create_queue() {
    queue* q = malloc(sizeof(queue));
    q->front = q->rear = NULL;
    return q;
}

static inline void enqueue(queue* q, int z, int y, int x) {
    queue_node* new_node = malloc(sizeof(queue_node));
    new_node->z = z;
    new_node->y = y;
    new_node->x = x;
    new_node->next = NULL;

    if (q->rear == NULL) {
        q->front = q->rear = new_node;
        return;
    }

    q->rear->next = new_node;
    q->rear = new_node;
}

static inline int dequeue(queue* q, int* z, int* y, int* x) {
    if (q->front == NULL) return 0;

    queue_node* temp = q->front;
    *z = temp->z;
    *y = temp->y;
    *x = temp->x;

    q->front = temp->next;
    if (q->front == NULL) q->rear = NULL;

    free(temp);
    return 1;
}

static inline void free_queue(queue* q) {
    while (q->front != NULL) {
        queue_node* temp = q->front;
        q->front = q->front->next;
        free(temp);
    }
    free(q);
}

// Main labeling function
static inline chunk* vs_chunk_label_components(chunk* input) {
    if (!input) return NULL;

    // Create output chunk
    chunk* output = vs_chunk_new(input->dims);
    if (!output) return NULL;

    // Initialize output to zeros
    size_t total_size = input->dims[0] * input->dims[1] * input->dims[2];
    memset(output->data, 0, total_size * sizeof(float));

    queue* q = create_queue();
    int current_label = 1;

    // Iterate through all voxels
    for (int z = 0; z < input->dims[0]; z++) {
        for (int y = 0; y < input->dims[1]; y++) {
            for (int x = 0; x < input->dims[2]; x++) {
                // Skip if already labeled or if input is zero
                if (vs_chunk_get(output, z, y, x) != 0 ||
                    vs_chunk_get(input, z, y, x) == 0) {
                    continue;
                }

                // Start new component
                vs_chunk_set(output, z, y, x, current_label);
                enqueue(q, z, y, x);

                // Process all connected voxels
                while (1) {
                    int curr_z, curr_y, curr_x;
                    if (!dequeue(q, &curr_z, &curr_y, &curr_x)) break;

                    // Check all 6 neighbors
                    const int offsets[6][3] = {
                        {-1, 0, 0}, {1, 0, 0},  // up, down
                        {0, -1, 0}, {0, 1, 0},  // front, back
                        {0, 0, -1}, {0, 0, 1}   // left, right
                    };

                    for (int i = 0; i < 6; i++) {
                        int nz = curr_z + offsets[i][0];
                        int ny = curr_y + offsets[i][1];
                        int nx = curr_x + offsets[i][2];

                        // Check bounds
                        if (nz < 0 || nz >= input->dims[0] ||
                            ny < 0 || ny >= input->dims[1] ||
                            nx < 0 || nx >= input->dims[2]) {
                            continue;
                        }

                        // If neighbor is non-zero in input and not yet labeled
                        if (vs_chunk_get(input, nz, ny, nx) != 0 &&
                            vs_chunk_get(output, nz, ny, nx) == 0) {
                            vs_chunk_set(output, nz, ny, nx, current_label);
                            enqueue(q, nz, ny, nx);
                        }
                    }
                }

                current_label++;
            }
        }
    }

    free_queue(q);
    return output;
}