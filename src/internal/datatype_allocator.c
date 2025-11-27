#include "internal.h"
#include <stdatomic.h>
#include <limits.h>
#include <stdlib.h>
#include <unistd.h>

static inline void free_stack_lock(struct SwiftNetMemoryAllocatorStack* const stack) {
    atomic_store_explicit(&stack->owner, ALLOCATOR_STACK_FREE, memory_order_release);
}

struct SwiftNetMemoryAllocatorStack* const find_free_pointer_stack(const struct SwiftNetMemoryAllocator* const allocator) {
    for (struct SwiftNetMemoryAllocatorStack* current_stack = atomic_load(&allocator->data.first_item); current_stack != NULL; current_stack = atomic_load_explicit(&current_stack->next, memory_order_acquire)) {
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

struct SwiftNetMemoryAllocatorStack* const find_valid_pointer_stack(const struct SwiftNetMemoryAllocator* const allocator) {
    for (struct SwiftNetMemoryAllocatorStack* current_stack = atomic_load(&allocator->data.first_item); current_stack != NULL; current_stack = atomic_load_explicit(&current_stack->next, memory_order_acquire)) {
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

struct SwiftNetMemoryAllocator allocator_create(const uint32_t item_size, const uint32_t chunk_item_amount) {
    void* const allocated_memory = malloc(item_size * chunk_item_amount);
    void* const pointers_memory = malloc(chunk_item_amount * sizeof(void*));

    if (unlikely(allocated_memory == NULL || pointers_memory == NULL)) {
        PRINT_ERROR("Failed to allocate memory");
        exit(EXIT_FAILURE);
    }

    struct SwiftNetMemoryAllocatorStack* const first_stack = malloc(sizeof(struct SwiftNetMemoryAllocatorStack));
    if (unlikely(first_stack == NULL)) {
        PRINT_ERROR("Failed to allocate memory");
        exit(EXIT_FAILURE);
    }

    first_stack->data = allocated_memory;
    first_stack->pointers = pointers_memory;
    first_stack->size = chunk_item_amount;
    first_stack->next = NULL;
    first_stack->previous = NULL;
    first_stack->owner = ALLOCATOR_STACK_FREE;

    struct SwiftNetMemoryAllocator new_allocator = (struct SwiftNetMemoryAllocator){
        .data = (struct SwiftNetChunkStorageManager){
            .first_item = first_stack,
            .last_item = first_stack,
        },
        .item_size = item_size,
        .chunk_item_amount = chunk_item_amount,
    };

    for (uint32_t i = 0; i < chunk_item_amount; i++) {
        (*((void **)pointers_memory + i) = (uint8_t*)allocated_memory + (i * item_size));
    }

    atomic_store_explicit(&new_allocator.creating_stack, STACK_CREATING_UNLOCKED, memory_order_release);

    return new_allocator;
}

static void create_new_stack(struct SwiftNetMemoryAllocator* const memory_allocator) {
    uint8_t creating_unlocked = STACK_CREATING_UNLOCKED;

    while (!atomic_compare_exchange_strong_explicit(&memory_allocator->creating_stack, &creating_unlocked, STACK_CREATING_LOCKED, memory_order_acquire, memory_order_relaxed)) {
        creating_unlocked = STACK_CREATING_UNLOCKED;

        usleep(100);

        continue;
    }

    const uint32_t chunk_item_amount = memory_allocator->chunk_item_amount;
    const uint32_t item_size = memory_allocator->item_size;

    void* const allocated_memory = malloc(memory_allocator->item_size * chunk_item_amount);
    void* const allocated_memory_pointers = malloc(chunk_item_amount * sizeof(void*));
    if (unlikely(allocated_memory == NULL || allocated_memory_pointers == NULL)) {
        PRINT_ERROR("Failed to allocate memory");
        exit(EXIT_FAILURE);
    }

    struct SwiftNetMemoryAllocatorStack* const stack = malloc(sizeof(struct SwiftNetMemoryAllocatorStack));
    if (unlikely(stack == NULL)) {
        PRINT_ERROR("Failed to allocate memory");
        exit(EXIT_FAILURE);
    }

    stack->pointers = allocated_memory_pointers;
    stack->data = allocated_memory;
    stack->size = chunk_item_amount;
    stack->previous = atomic_load(&memory_allocator->data.last_item);
    stack->next = NULL;
    stack->owner = ALLOCATOR_STACK_FREE;

    for (uint32_t i = 0; i < chunk_item_amount; i++) {
        ((void **)allocated_memory_pointers)[i] = (uint8_t*)allocated_memory + (i * item_size);
    }

    atomic_store_explicit(&((struct SwiftNetMemoryAllocatorStack*)atomic_load(&memory_allocator->data.last_item))->next, stack, memory_order_release);
    atomic_store_explicit(&memory_allocator->data.last_item, stack, memory_order_release);
    atomic_store_explicit(&memory_allocator->creating_stack, STACK_CREATING_UNLOCKED, memory_order_release);
}

void* allocator_allocate(struct SwiftNetMemoryAllocator* const memory_allocator) {
    struct SwiftNetMemoryAllocatorStack* const valid_stack = find_valid_pointer_stack(memory_allocator);
    if (valid_stack == NULL) {
        create_new_stack(memory_allocator);

        void* const res = allocator_allocate(memory_allocator);

        return res;
    }

    const uint32_t size = atomic_fetch_add(&valid_stack->size, -1);;

    void** const ptr_to_data = ((void**)valid_stack->pointers) + size - 1;

    void* item_ptr = *ptr_to_data;

    free_stack_lock(valid_stack);

    return item_ptr;
}

#ifdef SWIFT_NET_DEBUG
    static inline bool is_already_free(struct SwiftNetMemoryAllocator* const memory_allocator, void* const memory_location) {
        /*for (struct SwiftNetMemoryAllocatorStack* restrict stack = memory_allocator->free_memory_pointers.first_item; stack != NULL; stack = stack->next) {
            for (uint32_t i = 0; i < stack->size; i++) {
                if (*(((void**)stack->data) + i) == memory_location) {
                    return true;
                }
            }
        }*/

        return false;
    }
#endif

void allocator_free(struct SwiftNetMemoryAllocator* const memory_allocator, void* const memory_location) {
    #ifdef SWIFT_NET_DEBUG
        const bool already_free = is_already_free(memory_allocator, memory_location);

        if (already_free == true) {
            PRINT_ERROR("Pointer %p has already been freed", memory_location);
            exit(EXIT_FAILURE);
        }
    #endif

    struct SwiftNetMemoryAllocatorStack* const free_stack = find_free_pointer_stack(memory_allocator);
    if (free_stack == NULL) {
        create_new_stack(memory_allocator);

        allocator_free(memory_allocator, memory_location);

        return;
    }

    const uint32_t size = atomic_fetch_add(&free_stack->size, 1);

    ((void**)free_stack->pointers)[size] = memory_location;

    free_stack_lock(free_stack);
}

void allocator_destroy(struct SwiftNetMemoryAllocator* const memory_allocator) {
    for (struct SwiftNetMemoryAllocatorStack* current_stack_pointers = atomic_load(&memory_allocator->data.first_item); ; ) {
        free(current_stack_pointers->data);
        free(current_stack_pointers->pointers);

        struct SwiftNetMemoryAllocatorStack* const next_stack = atomic_load(&current_stack_pointers->next);
        if (next_stack == NULL) {
            free(current_stack_pointers);
            break;
        }

        free(current_stack_pointers);

        current_stack_pointers = next_stack;
    }
}
