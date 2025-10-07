#include "internal.h"

SwiftNetMemoryAllocator allocator_create(const uint32_t item_size, const uint32_t chunk_item_amount) {
    void* restrict const allocated_memory = malloc(item_size * chunk_item_amount);
    void* restrict const stack_allocated_memory = malloc(chunk_item_amount * sizeof(void*));

    SwiftNetMemoryAllocatorStack* restrict const first_stack_pointers = malloc(sizeof(SwiftNetMemoryAllocatorStack));
    first_stack_pointers->item_size = sizeof(void*);
    first_stack_pointers->data = allocated_memory;
    first_stack_pointers->size = chunk_item_amount;
    first_stack_pointers->capacity = chunk_item_amount;
    first_stack_pointers->next = NULL;
    first_stack_pointers->previous = NULL;

    SwiftNetMemoryAllocatorStack* restrict const first_stack_data = malloc(sizeof(SwiftNetMemoryAllocatorStack));
    first_stack_data->item_size = item_size;
    first_stack_data->data = allocated_memory;
    first_stack_data->size = 0;
    first_stack_data->capacity = chunk_item_amount;
    first_stack_data->next = NULL;
    first_stack_data->previous = NULL;

    SwiftNetMemoryAllocator new_allocator = (SwiftNetMemoryAllocator){
        .free_memory_pointers = (ChunkStorageManager){
            .current_chunk = first_stack_pointers,
            .first_item = first_stack_pointers,
            .last_item = first_stack_pointers
        },
        .data = (ChunkStorageManager){
            .first_item = first_stack_data,
            .last_item = first_stack_data,
            .current_chunk = first_stack_data
        }
    };

    for (uint32_t i = 0; i < chunk_item_amount; i++) {
        *(void**)(stack_allocated_memory + (i * sizeof(void*))) = allocated_memory + (i * item_size);
    }

    return new_allocator;
}

void* allocator_allocate(volatile SwiftNetMemoryAllocator* const memory_allocator) {
    volatile SwiftNetMemoryAllocatorStack* const current_stack = memory_allocator->free_memory_pointers.current_chunk;

    current_stack->size -= 1;

    void** restrict const ptr_to_data = current_stack->data + (sizeof(void*) * current_stack->size);

    void* item_ptr = *ptr_to_data;

    return item_ptr;
}

void allocator_free(volatile SwiftNetMemoryAllocator* const memory_allocator, void* const memory_location) {
    SwiftNetMemoryAllocatorStack* restrict const current_stack = memory_allocator->free_memory_pointers.current_chunk;

    *(void**)(current_stack->data + (sizeof(void*) * current_stack->size)) = memory_location;

    current_stack->size++;
}

void allocator_destroy(volatile SwiftNetMemoryAllocator* const memory_allocator) {
    SwiftNetMemoryAllocatorStack* restrict current_stack_pointers = memory_allocator->free_memory_pointers.first_item;

    while (1) {
        free(current_stack_pointers->data);

        SwiftNetMemoryAllocatorStack* restrict next_stack = current_stack_pointers->next;
        if (next_stack == NULL) {
            free(current_stack_pointers);
            break;
        }

        free(current_stack_pointers);

        current_stack_pointers = next_stack;
    }

    SwiftNetMemoryAllocatorStack* restrict current_stack_data = memory_allocator->data.first_item;

    while (1) {
        free(current_stack_data->data);

        SwiftNetMemoryAllocatorStack* restrict next_stack = current_stack_data->next;
        if (next_stack == NULL) {
            free(current_stack_data);
            break;
        }

        free(current_stack_data);

        current_stack_data = next_stack;
    }
}
