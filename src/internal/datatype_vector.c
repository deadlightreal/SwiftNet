#include "internal.h"
#include <stdatomic.h>

void vector_lock(SwiftNetVector* const vector) {
    uint8_t owner_none = 0;
    while(!atomic_compare_exchange_strong(&vector->locked, &owner_none, UINT8_MAX)) {
        owner_none = 0;
    }
}

void vector_unlock(SwiftNetVector* const vector) {
    atomic_store(&vector->locked, 0);
}

SwiftNetVector vector_create(const uint32_t starting_amount) {
    void* const data_ptr = malloc(sizeof(void*) * starting_amount);
    if (unlikely(data_ptr == NULL)) {
        fprintf(stderr, "Failed to malloc\n");
        exit(EXIT_FAILURE);
    }

    const SwiftNetVector new_vector = {
        .size = 0,
        .capacity = starting_amount,
        .data = data_ptr,
        .locked = 0
    };

    return new_vector;
}

void vector_destroy(volatile SwiftNetVector* const vector) {
    free(vector->data);
}

void vector_push(SwiftNetVector* const vector, void* const data) {
    const uint32_t size = vector->size;
    const uint32_t capacity = vector->capacity;

    if (size == capacity) {
        const uint32_t new_capacity = capacity * 2;

        vector->capacity = new_capacity;

        void** restrict const new_data_ptr = realloc(vector->data, sizeof(void*) * new_capacity);
        if (unlikely(new_data_ptr == NULL)) {
            fprintf(stderr, "Failed to malloc\n");
            exit(EXIT_FAILURE);
        }

        vector->data = new_data_ptr;
    }

    ((void**)vector->data)[size] = data;

    vector->size++;
}

void vector_remove(SwiftNetVector* const vector, const uint32_t index) {
    if (index < vector->size - 1) {
        memmove(
            ((void**)vector->data) + index,
            ((void**)vector->data) + index + 1,
            (vector->size - index - 1) * sizeof(void*)
        );
    }

    vector->size--;
}

void* vector_get(SwiftNetVector* const vector, const uint32_t index) {
    void** const data_ptr = ((void**)vector->data) + index;

    return *data_ptr;
}
