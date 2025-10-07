#include "internal.h"

SwiftNetStack stack_create(const uint32_t item_size, const uint32_t chunk_item_amount) {
    void* restrict const allocated_mem = malloc(item_size * chunk_item_amount);

    return (SwiftNetStack){
        .item_size = item_size,
        .size = 0,
        .capacity = chunk_item_amount,
        .data = allocated_mem
    };
}

void stack_destroy(const SwiftNetStack* restrict const stack) {
    free(stack->data);
}

void static inline increase_stack_capacity(SwiftNetStack* restrict const stack) {
    const uint32_t new_capacity = stack->capacity * 2;

    stack->data = realloc(stack->data, new_capacity * stack->item_size);
    
    stack->capacity = new_capacity;
}

void* stack_allocate(SwiftNetStack* restrict const stack) {
    void* newly_allocated_item_ptr = stack->data + (stack->size * stack->item_size);

    stack->size++;

    if (unlikely(stack->size > stack->capacity)) {
        increase_stack_capacity(stack);   
    }

    return newly_allocated_item_ptr;
}
