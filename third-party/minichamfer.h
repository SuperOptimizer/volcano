#pragma once

#include <math.h>
#include <float.h>
#include <stdlib.h>
#include <stdio.h>

#include "minilibs.h"

PRIVATE float squared_distance(const float* p1, const float* p2) {
    float dx = p1[0] - p2[0];
    float dy = p1[1] - p2[1];
    float dz = p1[2] - p2[2];
    return dx*dx + dy*dy + dz*dz;
}


PRIVATE float min_distance_to_set(const float* point, const float* point_set, int set_size) {
    float min_dist = FLT_MAX;

    for (int i = 0; i < set_size; i++) {
        float dist = squared_distance(point, &point_set[i * 3]);
        if (dist < min_dist) {
            min_dist = dist;
        }
    }
    return min_dist;
}

PUBLIC float chamfer_distance(const float* set1, int size1, const float* set2, int size2) {
    float sum1 = 0.0f;
    float sum2 = 0.0f;

    for (int i = 0; i < size1; i++) {
        sum1 += min_distance_to_set(&set1[i * 3], set2, size2);
    }

    for (int i = 0; i < size2; i++) {
        sum2 += min_distance_to_set(&set2[i * 3], set1, size1);
    }

    return sqrtf((sum1 / size1 + sum2 / size2) / 2.0f);
}

