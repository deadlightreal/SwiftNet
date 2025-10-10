#include "internal.h"
#include <stdatomic.h>
#include <limits.h>

static inline void free_stack_lock(SwiftNetMemoryAllocatorStack* restrict const stack) {
    atomic_store(&stack->owner, UINT_MAX);
}

SwiftNetMemoryAllocatorStack* find_free_pointer_stack(SwiftNetMemoryAllocator* restrict const allocator) {
    const unsigned int occupied = 0;

    for (SwiftNetMemoryAllocatorStack* restrict current_stack = allocator->free_memory_pointers.first_item; current_stack != NULL; current_stack = current_stack->next) {
        unsigned int thread_none = UINT_MAX;

        if (!atomic_compare_exchange_strong(&current_stack->owner, &thread_none, occupied)) {
            continue;
        }

        if (current_stack->size < current_stack->capacity) {
            return current_stack;
        } else {
            free_stack_lock(current_stack);

            continue;
        }
    }
    
    return NULL;
}

SwiftNetMemoryAllocatorStack* find_valid_pointer_stack(SwiftNetMemoryAllocator* restrict const allocator) {
    const unsigned int occupied = 0;

    for (SwiftNetMemoryAllocatorStack* restrict current_stack = allocator->free_memory_pointers.first_item; current_stack != NULL; current_stack = current_stack->next) {
        unsigned int thread_none = UINT_MAX;

        if (!atomic_compare_exchange_strong(&current_stack->owner, &thread_none, occupied)) {
            continue;
        }

        if (current_stack->size > 0) {
            return current_stack;
        } else {
            free_stack_lock(current_stack);

            continue;
        }
    }
    
    return NULL;
}

SwiftNetMemoryAllocator allocator_create(const uint32_t item_size, const uint32_t chunk_item_amount) {
    void* restrict const allocated_memory = malloc(item_size * chunk_item_amount);
    void* restrict const stack_allocated_memory = malloc(chunk_item_amount * sizeof(void*));

    if (unlikely(allocated_memory == NULL || stack_allocated_memory == NULL)) {
        fprintf(stderr, "Failed to allocate memory\n");
        exit(EXIT_FAILURE);
    }

    SwiftNetMemoryAllocatorStack* restrict const first_stack_pointers = malloc(sizeof(SwiftNetMemoryAllocatorStack));
    if (unlikely(first_stack_pointers == NULL)) {
        fprintf(stderr, "Failed to allocate memory\n");
        exit(EXIT_FAILURE);
    }

    first_stack_pointers->data = stack_allocated_memory;
    first_stack_pointers->size = chunk_item_amount;
    first_stack_pointers->capacity = chunk_item_amount;
    first_stack_pointers->next = NULL;
    first_stack_pointers->previous = NULL;

    atomic_store(&first_stack_pointers->owner, UINT_MAX);

    SwiftNetMemoryAllocatorStack* restrict const first_stack_data = malloc(sizeof(SwiftNetMemoryAllocatorStack));
    if (unlikely(first_stack_data == NULL)) {
        fprintf(stderr, "Failed to allocate memory\n");
        exit(EXIT_FAILURE);
    }

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
        .chunk_item_amount = chunk_item_amount,
    };

    for (uint32_t i = 0; i < chunk_item_amount; i++) {
        (*((void **)stack_allocated_memory + i) = (uint8_t*)allocated_memory + (i * item_size));
    }

    return new_allocator;
}

static void create_new_stack(volatile SwiftNetMemoryAllocator* const memory_allocator) {
    const uint32_t chunk_item_amount = memory_allocator->chunk_item_amount;
    const uint32_t item_size = memory_allocator->item_size;

    void* restrict const allocated_memory = malloc(memory_allocator->item_size * chunk_item_amount);
    void* restrict const stack_allocated_memory = malloc(chunk_item_amount * sizeof(void*));
    if (unlikely(allocated_memory == NULL || stack_allocated_memory == NULL)) {
        fprintf(stderr, "Failed to allocate memory\n");
        exit(EXIT_FAILURE);
    }

    SwiftNetMemoryAllocatorStack* restrict const stack_pointers = malloc(sizeof(SwiftNetMemoryAllocatorStack));
    if (unlikely(stack_pointers == NULL)) {
        fprintf(stderr, "Failed to allocate memory\n");
        exit(EXIT_FAILURE);
    }

    stack_pointers->data = stack_allocated_memory;
    stack_pointers->size = chunk_item_amount;
    stack_pointers->capacity = chunk_item_amount;
    stack_pointers->previous = memory_allocator->free_memory_pointers.last_item;
    stack_pointers->next = NULL;

    ((SwiftNetMemoryAllocatorStack*)memory_allocator->free_memory_pointers.last_item)->next = stack_pointers;
    memory_allocator->free_memory_pointers.last_item = stack_pointers;

    atomic_store(&stack_pointers->owner, UINT_MAX);

    SwiftNetMemoryAllocatorStack* restrict const stack_data = malloc(sizeof(SwiftNetMemoryAllocatorStack));
    if (unlikely(stack_data == NULL)) {
        fprintf(stderr, "Failed to allocate memory\n");
        exit(EXIT_FAILURE);
    }

    stack_data->data = allocated_memory;
    stack_data->size = 0;
    stack_data->capacity = chunk_item_amount;
    stack_data->next = NULL;
    stack_data->previous = memory_allocator->data.last_item;

    ((SwiftNetMemoryAllocatorStack*)memory_allocator->data.last_item)->next = stack_data;
    memory_allocator->data.last_item = stack_data;

    for (uint32_t i = 0; i < chunk_item_amount; i++) {
        ((void **)stack_allocated_memory)[i] = (uint8_t*)allocated_memory + (i * item_size);
    }
}

void* allocator_allocate(volatile SwiftNetMemoryAllocator* const memory_allocator) {
    volatile SwiftNetMemoryAllocatorStack* valid_stack = find_valid_pointer_stack(memory_allocator);
    if (valid_stack == NULL) {
        create_new_stack(memory_allocator);

        void* res = allocator_allocate(memory_allocator);

        return res;
    }

    valid_stack->size -= 1;

    void** restrict const ptr_to_data = ((void**)valid_stack->data) + valid_stack->size;

    void* item_ptr = *ptr_to_data;

    free_stack_lock(valid_stack);

    return item_ptr;
}

SwiftNetDebug(
    static inline bool is_already_free(SwiftNetMemoryAllocator* restrict const memory_allocator, void* const memory_location) {
        for (SwiftNetMemoryAllocatorStack* restrict stack = memory_allocator->free_memory_pointers.first_item; stack != NULL; stack = stack->next) {
            for (uint32_t i = 0; i < stack->size; i++) {
                if (*(((void**)stack->data) + i) == memory_location) {
                    return true;
                }
            }
        }

        return false;
    }
)

void allocator_free(SwiftNetMemoryAllocator* restrict const memory_allocator, void* const memory_location) {
    ChunkStorageManager* restrict const free_pointers_chunk_storage = &memory_allocator->free_memory_pointers;

    SwiftNetDebug(
        const bool already_free = is_already_free(memory_allocator, memory_location);

        if (already_free) {
            fprintf(stderr, "Pointer %p has already been freed\n", memory_location);
            exit(EXIT_FAILURE);
        }
    )

    SwiftNetMemoryAllocatorStack* free_stack = find_free_pointer_stack(memory_allocator);
    if (free_stack == NULL) {
        create_new_stack(memory_allocator);

        allocator_free(memory_allocator, memory_location);

        return;
    }


    ((void**)free_stack->data)[free_stack->size] = memory_location;

    free_stack->size++;

    free_stack_lock(free_stack);
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
