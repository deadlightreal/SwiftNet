#include "internal.h"

SwiftNetVector vector_create(const uint32_t starting_amount) {
    void* restrict const data_ptr = malloc(sizeof(void*) * starting_amount);
    if (unlikely(data_ptr == NULL)) {
        fprintf(stderr, "Failed to malloc\n");
        exit(EXIT_FAILURE);
    }

    const SwiftNetVector new_vector = {
        .size = 0,
        .capacity = starting_amount,
        .data = data_ptr,
    };

    return new_vector;
}

void vector_destroy(SwiftNetVector* vector) {
    free(vector->data);
}

void vector_push(SwiftNetVector* vector, void* data) {
    if (vector->size == vector->capacity) {
        vector->capacity *= 2;

        void* restrict const new_data_ptr = realloc(vector->data, sizeof(void*) * vector->capacity);
        if (unlikely(new_data_ptr == NULL)) {
            fprintf(stderr, "Failed to malloc\n");
            exit(EXIT_FAILURE);
        }

        vector->data = new_data_ptr;
    }

    void** const restrict new_item_pointer = ((void**)&vector->data) + vector->size;

    memcpy(new_item_pointer, data, sizeof(void*));

    vector->size++;
}

void vector_remove(SwiftNetVector* vector, const uint32_t index) {
    if (index < vector->size - 1) {
        memmove(
            ((void**)vector->data) + index,
            ((void**)vector->data) + index + 1,
            (vector->size - index - 1) * sizeof(void*)
        );
    }

    vector->size--;
}

void* vector_get(SwiftNetVector* vector, const uint32_t index) {
    void** restrict const data_ptr = ((void**)vector->data) + index;

    return *data_ptr;
}
