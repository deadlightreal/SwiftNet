#include "internal.h"
#include <stdatomic.h>
#include <limits.h>
#include <unistd.h>

static inline void free_stack_lock(SwiftNetMemoryAllocatorStack* const stack) {
    atomic_store_explicit(&stack->owner, ALLOCATOR_STACK_FREE, memory_order_release);
}

SwiftNetMemoryAllocatorStack* const find_free_pointer_stack(const SwiftNetMemoryAllocator* const allocator) {
    for (SwiftNetMemoryAllocatorStack* current_stack = atomic_load(&allocator->free_memory_pointers.first_item); current_stack != NULL; current_stack = atomic_load_explicit(&current_stack->next, memory_order_acquire)) {
        uint8_t thread_none = ALLOCATOR_STACK_FREE;

        if (!atomic_compare_exchange_strong_explicit(
                &current_stack->owner,
                &thread_none,
                ALLOCATOR_STACK_OCCUPIED,
                memory_order_acquire,
                memory_order_relaxed))
        {
            continue;
        }

        if (atomic_load(&current_stack->size) < allocator->chunk_item_amount) {
            return current_stack;
        } else {
            free_stack_lock(current_stack);

            continue;
        }
    }
    
    return NULL;
}

SwiftNetMemoryAllocatorStack* const find_valid_pointer_stack(const SwiftNetMemoryAllocator* const allocator) {
    for (SwiftNetMemoryAllocatorStack* current_stack = atomic_load(&allocator->free_memory_pointers.first_item); current_stack != NULL; current_stack = atomic_load_explicit(&current_stack->next, memory_order_acquire)) {
        uint8_t thread_none = ALLOCATOR_STACK_FREE;

        if (!atomic_compare_exchange_strong_explicit(&current_stack->owner, &thread_none, ALLOCATOR_STACK_OCCUPIED, memory_order_acquire, memory_order_relaxed)) {
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
    void* const allocated_memory = malloc(item_size * chunk_item_amount);
    void* const stack_allocated_memory = malloc(chunk_item_amount * sizeof(void*));

    if (unlikely(allocated_memory == NULL || stack_allocated_memory == NULL)) {
        fprintf(stderr, "Failed to allocate memory\n");
        exit(EXIT_FAILURE);
    }

    SwiftNetMemoryAllocatorStack* const first_stack_pointers = malloc(sizeof(SwiftNetMemoryAllocatorStack));
    if (unlikely(first_stack_pointers == NULL)) {
        fprintf(stderr, "Failed to allocate memory\n");
        exit(EXIT_FAILURE);
    }

    first_stack_pointers->data = stack_allocated_memory;
    first_stack_pointers->size = chunk_item_amount;
    first_stack_pointers->next = NULL;
    first_stack_pointers->previous = NULL;
    first_stack_pointers->owner = ALLOCATOR_STACK_FREE;

    SwiftNetMemoryAllocatorStack* const first_stack_data = malloc(sizeof(SwiftNetMemoryAllocatorStack));
    if (unlikely(first_stack_data == NULL)) {
        fprintf(stderr, "Failed to allocate memory\n");
        exit(EXIT_FAILURE);
    }

    first_stack_data->data = allocated_memory;
    first_stack_data->size = 0;
    first_stack_data->next = NULL;
    first_stack_data->previous = NULL;

    SwiftNetMemoryAllocator new_allocator = (SwiftNetMemoryAllocator){
        .free_memory_pointers = (SwiftNetChunkStorageManager){
            .first_item = first_stack_pointers,
            .last_item = first_stack_pointers,
        },
        .data = (SwiftNetChunkStorageManager){
            .first_item = first_stack_data,
            .last_item = first_stack_data,
        },
        .item_size = item_size,
        .chunk_item_amount = chunk_item_amount,
    };

    for (uint32_t i = 0; i < chunk_item_amount; i++) {
        (*((void **)stack_allocated_memory + i) = (uint8_t*)allocated_memory + (i * item_size));
    }

    new_allocator.creating_stack = STACK_CREATING_UNLOCKED;

    return new_allocator;
}

static void create_new_stack(SwiftNetMemoryAllocator* const memory_allocator) {
    uint8_t creating_unlocked = STACK_CREATING_UNLOCKED;

    while (!atomic_compare_exchange_strong_explicit(&memory_allocator->creating_stack, &creating_unlocked, STACK_CREATING_LOCKED, memory_order_acquire, memory_order_relaxed)) {
        creating_unlocked = STACK_CREATING_UNLOCKED;

        usleep(100);

        continue;
    }

    const uint32_t chunk_item_amount = memory_allocator->chunk_item_amount;
    const uint32_t item_size = memory_allocator->item_size;

    void* const allocated_memory = malloc(memory_allocator->item_size * chunk_item_amount);
    void* const stack_allocated_memory = malloc(chunk_item_amount * sizeof(void*));
    if (unlikely(allocated_memory == NULL || stack_allocated_memory == NULL)) {
        fprintf(stderr, "Failed to allocate memory\n");
        exit(EXIT_FAILURE);
    }

    SwiftNetMemoryAllocatorStack* const stack_pointers = malloc(sizeof(SwiftNetMemoryAllocatorStack));
    if (unlikely(stack_pointers == NULL)) {
        fprintf(stderr, "Failed to allocate memory\n");
        exit(EXIT_FAILURE);
    }

    stack_pointers->data = stack_allocated_memory;
    stack_pointers->size = chunk_item_amount;
    stack_pointers->previous = atomic_load(&memory_allocator->free_memory_pointers.last_item);
    stack_pointers->next = NULL;

    atomic_store(&((SwiftNetMemoryAllocatorStack*)atomic_load(&memory_allocator->free_memory_pointers.last_item))->next, stack_pointers);
    atomic_store(&memory_allocator->free_memory_pointers.last_item, stack_pointers);

    atomic_store(&stack_pointers->owner, ALLOCATOR_STACK_FREE);

    SwiftNetMemoryAllocatorStack* const stack_data = malloc(sizeof(SwiftNetMemoryAllocatorStack));
    if (unlikely(stack_data == NULL)) {
        fprintf(stderr, "Failed to allocate memory\n");
        exit(EXIT_FAILURE);
    }

    stack_data->data = allocated_memory;
    stack_data->size = 0;
    stack_data->next = NULL;
    stack_data->previous = atomic_load(&memory_allocator->data.last_item);

    atomic_store(&((SwiftNetMemoryAllocatorStack*)atomic_load(&memory_allocator->data.last_item))->next, stack_data);
    atomic_store(&memory_allocator->data.last_item, stack_data);

    for (uint32_t i = 0; i < chunk_item_amount; i++) {
        ((void **)stack_allocated_memory)[i] = (uint8_t*)allocated_memory + (i * item_size);
    }

    atomic_store(&memory_allocator->creating_stack, STACK_CREATING_UNLOCKED);
}

void* allocator_allocate(SwiftNetMemoryAllocator* const memory_allocator) {
    SwiftNetMemoryAllocatorStack* const valid_stack = find_valid_pointer_stack(memory_allocator);
    if (valid_stack == NULL) {
        create_new_stack(memory_allocator);

        void* const res = allocator_allocate(memory_allocator);

        return res;
    }

    const uint32_t size = atomic_fetch_add(&valid_stack->size, -1);;

    void** const ptr_to_data = ((void**)valid_stack->data) + size - 1;

    void* item_ptr = *ptr_to_data;

    free_stack_lock(valid_stack);

    return item_ptr;
}

#ifdef SWIFT_NET_DEBUG
    static inline bool is_already_free(SwiftNetMemoryAllocator* const memory_allocator, void* const memory_location) {
        /*for (SwiftNetMemoryAllocatorStack* restrict stack = memory_allocator->free_memory_pointers.first_item; stack != NULL; stack = stack->next) {
            for (uint32_t i = 0; i < stack->size; i++) {
                if (*(((void**)stack->data) + i) == memory_location) {
                    return true;
                }
            }
        }*/

        return false;
    }
#endif

void allocator_free(SwiftNetMemoryAllocator* const memory_allocator, void* const memory_location) {
    #ifdef SWIFT_NET_DEBUG
        const bool already_free = is_already_free(memory_allocator, memory_location);

        if (already_free == true) {
            fprintf(stderr, "Pointer %p has already been freed\n", memory_location);
            exit(EXIT_FAILURE);
        }
    #endif

    SwiftNetMemoryAllocatorStack* const free_stack = find_free_pointer_stack(memory_allocator);
    if (free_stack == NULL) {
        create_new_stack(memory_allocator);

        allocator_free(memory_allocator, memory_location);

        return;
    }

    const uint32_t size = atomic_fetch_add(&free_stack->size, 1);

    ((volatile void**)free_stack->data)[size] = memory_location;

    free_stack_lock(free_stack);
}

void allocator_destroy(SwiftNetMemoryAllocator* const memory_allocator) {
    for (SwiftNetMemoryAllocatorStack* current_stack_pointers = atomic_load(&memory_allocator->free_memory_pointers.first_item); ; ) {
        free(current_stack_pointers->data);

        SwiftNetMemoryAllocatorStack* const next_stack = atomic_load(&current_stack_pointers->next);
        if (next_stack == NULL) {
            free(current_stack_pointers);
            break;
        }

        free(current_stack_pointers);

        current_stack_pointers = next_stack;
    }

    
    for (SwiftNetMemoryAllocatorStack* current_stack_data = atomic_load(&memory_allocator->data.first_item); ; ) {
        free(current_stack_data->data);

        SwiftNetMemoryAllocatorStack* const next_stack = atomic_load(&current_stack_data->next);
        if (next_stack == NULL) {
            free(current_stack_data);
            break;
        }

        free(current_stack_data);

        current_stack_data = next_stack;
    }
}
