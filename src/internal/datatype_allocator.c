#include "internal.h"

SwiftNetMemoryAllocator allocator_create(const uint32_t item_size, const uint32_t chunk_item_amount) {
    void* restrict const allocated_memory = malloc(item_size * chunk_item_amount);
    void* restrict const stack_allocated_memory = malloc(chunk_item_amount * sizeof(void*));

    SwiftNetMemoryAllocatorStack* restrict const first_stack_pointers = malloc(sizeof(SwiftNetMemoryAllocatorStack));
    first_stack_pointers->data = stack_allocated_memory;
    first_stack_pointers->size = chunk_item_amount;
    first_stack_pointers->capacity = chunk_item_amount;
    first_stack_pointers->next = NULL;
    first_stack_pointers->previous = NULL;

    SwiftNetMemoryAllocatorStack* restrict const first_stack_data = malloc(sizeof(SwiftNetMemoryAllocatorStack));
    first_stack_data->data = allocated_memory;
    first_stack_data->size = 0;
    first_stack_data->capacity = chunk_item_amount;
    first_stack_data->next = NULL;
    first_stack_data->previous = NULL;

    SwiftNetMemoryAllocator new_allocator = (SwiftNetMemoryAllocator){
        .free_memory_pointers = (ChunkStorageManager){
            .current_chunk = first_stack_pointers,
            .first_item = first_stack_pointers,
            .last_item = first_stack_pointers,
        },
        .data = (ChunkStorageManager){
            .first_item = first_stack_data,
            .last_item = first_stack_data,
            .current_chunk = first_stack_data,
        },
        .item_size = item_size,
        .chunk_item_amount = chunk_item_amount
    };

    for (uint32_t i = 0; i < chunk_item_amount; i++) {
        ((void **)stack_allocated_memory)[i] = (uint8_t *)allocated_memory + (i * item_size);
    }

    return new_allocator;
}

void* allocator_allocate(volatile SwiftNetMemoryAllocator* const memory_allocator) {
    volatile SwiftNetMemoryAllocatorStack* current_stack = memory_allocator->free_memory_pointers.current_chunk;

    if (current_stack->size == 0) {
        if (current_stack->previous == NULL) {
            const uint32_t chunk_item_amount = memory_allocator->chunk_item_amount;
            const uint32_t item_size = memory_allocator->item_size;

            void* restrict const allocated_memory = malloc(memory_allocator->item_size * chunk_item_amount);
            void* restrict const stack_allocated_memory = malloc(chunk_item_amount * sizeof(void*));

            SwiftNetMemoryAllocatorStack* restrict const stack_pointers = malloc(sizeof(SwiftNetMemoryAllocatorStack));
            stack_pointers->data = stack_allocated_memory;
            stack_pointers->size = chunk_item_amount;
            stack_pointers->capacity = chunk_item_amount;
            stack_pointers->next = (void*)current_stack;
            stack_pointers->previous = NULL;

            SwiftNetMemoryAllocatorStack* restrict const stack_data = malloc(sizeof(SwiftNetMemoryAllocatorStack));
            stack_data->data = allocated_memory;
            stack_data->size = 0;
            stack_data->capacity = chunk_item_amount;
            stack_data->next = NULL;
            stack_data->previous = memory_allocator->data.last_item;

            for (uint32_t i = 0; i < chunk_item_amount; i++) {
                ((void **)stack_allocated_memory)[i] = (uint8_t *)allocated_memory + (i * item_size);
            }

            current_stack->previous = stack_pointers;

            memory_allocator->free_memory_pointers.first_item = stack_pointers;
            memory_allocator->free_memory_pointers.current_chunk = stack_pointers;

            current_stack = stack_pointers;

            ((SwiftNetMemoryAllocatorStack*)&memory_allocator->data.last_item)->next = stack_data;

            memory_allocator->data.last_item = stack_data;
        } else {
            memory_allocator->free_memory_pointers.current_chunk = current_stack->previous;

            void* res = allocator_allocate(memory_allocator);

            return res;
        }
    }

    current_stack->size -= 1;

    void** restrict const ptr_to_data = current_stack->data + (sizeof(void*) * current_stack->size);

    void* item_ptr = *ptr_to_data;

    return item_ptr;
}

void allocator_free(SwiftNetMemoryAllocator* restrict const memory_allocator, void* const memory_location) {
    ChunkStorageManager* restrict const free_pointers_chunk_storage = &memory_allocator->free_memory_pointers;
    SwiftNetMemoryAllocatorStack* restrict current_stack = free_pointers_chunk_storage->current_chunk;

    // temp until issues with double free is resolved!
    memset(memory_location, 0x00, memory_allocator->item_size);

    if (current_stack->size >= current_stack->capacity) {
        if (current_stack->next == NULL) {
            fprintf(stderr, "failed to free in allocator");
            exit(EXIT_FAILURE);
        } else {
            free_pointers_chunk_storage->current_chunk = current_stack->next;

            current_stack = free_pointers_chunk_storage->current_chunk;
        }
    }

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
