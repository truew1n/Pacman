#pragma once

#include <stdlib.h>
#include <stdint.h>


typedef struct point_t {
    int32_t x;
    int32_t y;
} point_t;

typedef struct vector_t {
    int32_t size;
    point_t *memory;
} vector_t;

#define ZERO_POINT {0, 0};
#define EMPTY_VECTOR {0, NULL};

vector_t new_vector(int32_t size)
{
    vector_t vector = {size, (point_t *) malloc(sizeof(point_t) * size)};
    return vector;
}

point_t vector_get(vector_t *vector, int32_t index)
{
    if(!(index >= 0 && index < vector->size)) exit(-1);
    return *(vector->memory + index);
}

void vector_add(vector_t *vector, point_t point)
{
    point_t *new_memory = (point_t *) malloc(sizeof(point_t) * (vector->size + 1));
    for(int32_t i = 0; i < vector->size; ++i) {
        *(new_memory + i) = *(vector->memory + i);
    }
    *(new_memory + vector->size++) = point;
    free(vector->memory);
    vector->memory = new_memory;
}

void vector_remove(vector_t *vector, int32_t index)
{
    point_t *new_memory = (point_t *) malloc(sizeof(point_t) * (vector->size - 1));
    for(int32_t i = 0; i < index; ++i) {
        *(new_memory + i) = *(vector->memory + i);
    }
    for(int32_t i = index; i < (vector->size - 1); ++i) {
        *(new_memory + i) = *(vector->memory + i + 1);
    }
    free(vector->memory);
    vector->size--;
    vector->memory = new_memory;
}

void vector_free(vector_t *vector)
{
    free(vector->memory);
    vector->size = 0;
}