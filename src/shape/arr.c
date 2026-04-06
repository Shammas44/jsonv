#include "arr.h"
#include <stdlib.h>

void arr_free(Arr *a) {
    // Release references to all contained values before freeing the array
    for (int i = 0; i < a->length; i++) {
        value_release(a->items[i]);
    }
    free(a->items);
    free(a);
}

/* ------------------- Array ------------------- */

Arr *arr_new(void) {
    Arr *a = (Arr *)calloc(1, sizeof(Arr));
    a->items = NULL;
    a->capacity = 0;
    a->length = 0;
    a->refcount = 0;
    return a;
}

void arr_ensure_capacity(Arr *a, int needed) {
    if (a->capacity >= needed)
        return;
        
    int newcap = a->capacity ? a->capacity : 4;
    while (newcap < needed)
        newcap *= 2;

    a->items = (Value *)realloc(a->items, (size_t)newcap * sizeof(Value));
    
    // Initialize new memory to undefined to prevent reading garbage data
    for (int i = a->capacity; i < newcap; i++) {
        a->items[i] = val_undefined();
    }
    a->capacity = newcap;
}

/*
  Push value:
  - Ensures capacity for at least one more element
  - Retains the new value and appends it to the end
*/
void arr_push(Arr *a, Value v) {
    arr_ensure_capacity(a, a->length + 1);
    
    value_retain(v); // Hold new reference
    a->items[a->length] = v;
    a->length++;
}

/*
  Set value at index:
  - If index is out of current bounds, grows the array
  - Safely releases any old value and retains the new one
*/
void arr_set(Arr *a, int index, Value v) {
    if (index < 0) return; // Prevent negative indices

    // If setting beyond the current length, expand the array
    if (index >= a->length) {
        arr_ensure_capacity(a, index + 1);
        a->length = index + 1; // Update length to include the new index
    }

    // Existing property -> overwrite
    Value old = a->items[index];
    value_retain(v); // Hold new reference first
    a->items[index] = v;
    value_release(old); // Release old reference
}

/*
  Get value at index:
  - Validates bounds before returning the value
*/
int arr_get(Arr *a, int index, Value *out) {
    if (index < 0 || index >= a->length)
        return 0; // Out of bounds
        
    *out = a->items[index];
    return 1;
}
