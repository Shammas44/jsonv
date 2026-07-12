# Custom Arena Allocator Documentation

The `jsonv` library allows consumers to replace the default chained arena allocator with their own custom memory allocation strategy. This is useful for:
*   Integrating the library into custom runtimes (e.g., game engines, trading loops, or embedded kernels).
*   Enforcing custom thread-local or static buffer allocation policies to achieve zero heap-allocation overhead.
*   Enabling detailed memory usage logging, metrics tracking, or custom OOM handling.

---

## 1. The Interface

The custom allocator interface is defined in `arena.h` via the `Jsonv_Arena_Ops` struct and the `jsonv_arena_new_custom` constructor function:

```c
typedef struct Jsonv_Arena_Ops {
    void *(*alloc)(void *user_data, size_t size);
    void (*reset)(void *user_data);
    void (*reset_to)(void *user_data, size_t keep_size); // Optional: can be NULL
    void (*destroy)(void *user_data);                  // Optional: can be NULL
} Jsonv_Arena_Ops;

/**
 * @description Create a wrapper arena around a user-defined custom allocator
 * @param ops       Struct of function pointers representing allocator operations
 * @param user_data Arbitrary pointer passed back to the callbacks
 * @return Opaque pointer to the wrapped arena
 */
JSONV_API Jsonv_Arena *jsonv_arena_new_custom(const Jsonv_Arena_Ops *ops, void *user_data);
```

### Callback Operations:
1.  **`alloc`**: Request `size` bytes. The custom allocator must return a pointer aligned to 16 bytes. If the allocation fails, return `NULL`.
2.  **`reset`**: Resets the arena (equivalent to resetting memory offset to 0).
3.  **`reset_to`**: Resets the arena to a specific `keep_size` bytes. If your custom allocator does not support resetting to a specific offset, you can set this function pointer to `NULL`. The library will fallback to calling `reset` if `keep_size` is 0.
4.  **`destroy`**: Cleans up the custom allocator state (if applicable). Set to `NULL` if your custom allocator's memory is managed externally (e.g., stack-allocated).

---

## 2. Examples

### Example 2.1: Simple `malloc` / `free` Custom Allocator

This implementation delegates allocations directly to the standard library's `malloc` and tracks total allocated blocks to free them on destruction.

```c
#include "jsonv.h"
#include <stdlib.h>
#include <stdio.h>

typedef struct MallocTracker {
    void **ptrs;
    size_t count;
    size_t capacity;
} MallocTracker;

void *tracker_alloc(void *user_data, size_t size) {
    MallocTracker *tracker = (MallocTracker *)user_data;
    
    // Ensure 16-byte alignment
    size_t aligned_size = (size + 15) & ~15;
    void *ptr = malloc(aligned_size);
    if (!ptr) return NULL;
    
    // Record pointer
    if (tracker->count >= tracker->capacity) {
        tracker->capacity = tracker->capacity == 0 ? 16 : tracker->capacity * 2;
        tracker->ptrs = realloc(tracker->ptrs, tracker->capacity * sizeof(void *));
    }
    tracker->ptrs[tracker->count++] = ptr;
    return ptr;
}

void tracker_reset(void *user_data) {
    MallocTracker *tracker = (MallocTracker *)user_data;
    for (size_t i = 0; i < tracker->count; i++) {
        free(tracker->ptrs[i]);
    }
    tracker->count = 0;
}

void tracker_destroy(void *user_data) {
    MallocTracker *tracker = (MallocTracker *)user_data;
    tracker_reset(tracker);
    free(tracker->ptrs);
    free(tracker);
}

void run_malloc_tracker() {
    MallocTracker *tracker = calloc(1, sizeof(MallocTracker));
    
    Jsonv_Arena_Ops ops = {
        .alloc = tracker_alloc,
        .reset = tracker_reset,
        .reset_to = NULL, // Fallback to reset
        .destroy = tracker_destroy
    };
    
    Jsonv_Arena *arena = jsonv_arena_new_custom(&ops, tracker);
    
    // Use arena inside jsonv context...
    Jsonv_Context *ctx = jsonv_ctx_new(arena, &config, NULL);
    
    // Cleanup
    jsonv_arena_destroy(arena); // Triggers tracker_destroy and frees the wrapper
}
```

---

### Example 2.2: Fixed‑Size Stack/Buffer Allocator (Zero Allocations)

This implementation uses a pre-allocated fixed buffer, providing absolute deterministic timing and zero dynamic heap calls.

```c
#include "jsonv.h"
#include <stdint.h>

#define STACK_CAPACITY (64 * 1024) // 64 KB

typedef struct StackAllocator {
    uint8_t buffer[STACK_CAPACITY];
    size_t offset;
} StackAllocator;

void *stack_alloc(void *user_data, size_t size) {
    StackAllocator *stack = (StackAllocator *)user_data;
    size_t aligned_size = (size + 15) & ~15;
    
    if (stack->offset + aligned_size > STACK_CAPACITY) {
        return NULL; // Out of memory
    }
    
    void *ptr = &stack->buffer[stack->offset];
    stack->offset += aligned_size;
    return ptr;
}

void stack_reset(void *user_data) {
    StackAllocator *stack = (StackAllocator *)user_data;
    stack->offset = 0;
}

void stack_reset_to(void *user_data, size_t keep_size) {
    StackAllocator *stack = (StackAllocator *)user_data;
    stack->offset = (keep_size + 15) & ~15;
}

void run_stack_allocator() {
    StackAllocator stack = { .offset = 0 };
    
    Jsonv_Arena_Ops ops = {
        .alloc = stack_alloc,
        .reset = stack_reset,
        .reset_to = stack_reset_to,
        .destroy = NULL // No destroy needed for stack memory
    };
    
    Jsonv_Arena *arena = jsonv_arena_new_custom(&ops, &stack);
    
    // Compile schema / parse data using the stack-based arena
    // ...
    
    jsonv_arena_destroy(arena); // Only frees the wrapper container
}
```

---

## 3. Error Handling and Decoupling

To keep your custom allocator code completely decoupled and reusable across other libraries, it does not need to know about `jsonv_last_arena_error` or include any `jsonv` headers. 

### Recommended Design:
1.  **Failure Indication**: If an allocation fails in your custom allocator callback, simply return `NULL`.
2.  **Library-Side Mapping**: The `jsonv` library intercepts the `NULL` return and automatically sets its internal thread-local error to `JSONV_ARENA_ERR_ALLOC`, triggering standard graceful out-of-memory handler recovery or exceptions.
3.  **Custom / Granular Errors**: If you need to track specific allocation failure details (e.g., hitting a custom quota or detecting corruption), store this status inside your own custom context structure (`user_data`).

### Example of Decoupled Custom Error Handling:

```c
// Completely generic custom allocator context (no jsonv dependency)
typedef struct MyCustomArena {
    uint8_t buffer[1024];
    size_t offset;
    int custom_error; // 0 = OK, 1 = QUOTA_EXCEEDED
} MyCustomArena;

void *my_alloc(void *user_data, size_t size) {
    MyCustomArena *arena = (MyCustomArena *)user_data;
    size_t aligned = (size + 15) & ~15;
    if (arena->offset + aligned > sizeof(arena->buffer)) {
        arena->custom_error = 1; // Mark quota exceeded in user context
        return NULL;
    }
    void *ptr = &arena->buffer[arena->offset];
    arena->offset += aligned;
    return ptr;
}

// User-side verification
void run() {
    MyCustomArena my_arena = {0};
    Jsonv_Arena_Ops ops = { .alloc = my_alloc };
    Jsonv_Arena *arena = jsonv_arena_new_custom(&ops, &my_arena);
    
    Jsonv_Context *ctx = jsonv_ctx_new(arena, &config, NULL);
    if (!ctx) {
        if (my_arena.custom_error == 1) {
            printf("Custom Allocator: Quota Exceeded!\n");
        } else {
            printf("Generic allocation failure.\n");
        }
    }
    jsonv_arena_destroy(arena);
}
```


