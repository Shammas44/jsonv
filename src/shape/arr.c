#include "shape.internal.h"
#include "mem.h"
#include <stdlib.h>
#include <string.h>

void arr_free(Arr *a) {
  /*#region*/
  // Memory is automatically collected on Arena resets/destructions, 
  // so this acts strictly as a safe no-op to comply with the malloc/free ban.
  (void)a;
  /*#endregion*/
}

/* ------------------- Array ------------------- */

Arr *arr_new(Jsonv_Arena *arena) {
  /*#region*/
  // Conforms strictly to memory laws: allocates directly on the Arena memory pool
  Arr *a = (Arr *)jsonv_arena_alloc(arena, sizeof(Arr));
  if (!a) return NULL;
  a->items = NULL;
  a->capacity = 0;
  a->length = 0;
  a->refcount = 0;
  return a;
  /*#endregion*/
}

Jsonv_Arr *jsonv_arr_new(Jsonv_Arena *arena) {
  /*#region*/
  return arr_new(arena);
  /*#endregion*/
}

void arr_ensure_capacity(Jsonv_Arena *arena, Arr *a, int needed) {
  /*#region*/
  if (a->capacity >= needed)
    return;
      
  int newcap = a->capacity ? a->capacity : 4;
  while (newcap < needed)
    newcap *= 2;

  // Conforms strictly to memory laws: allocates a new dynamic slot buffer in the Arena
  Value *new_items = (Value *)jsonv_arena_alloc(arena, (size_t)newcap * sizeof(Value));
  if (!new_items) return;
  
  // Copy old items
  if (a->items && a->capacity > 0) {
    memcpy(new_items, a->items, (size_t)a->capacity * sizeof(Value));
  }

  // Initialize new memory to undefined to prevent reading garbage data
  for (int i = a->capacity; i < newcap; i++) {
    new_items[i] = jsonv_val_undefined();
  }

  a->items = new_items;
  a->capacity = newcap;
  /*#endregion*/
}

void arr_set(Jsonv_Arena *arena, Arr *a, int index, Value v) {
  /*#region*/
  if (index < 0) return; // Prevent negative indices

  // If setting beyond the current length, expand the array
  if (index >= a->length) {
    arr_ensure_capacity(arena, a, index + 1);
    a->length = index + 1; // Update length to include the new index
  }

  // Existing property -> overwrite
  Value old = a->items[index];
  value_retain(v); // Hold new reference first
  a->items[index] = v;
  value_release(old); // Release old reference
  /*#endregion*/
}

void jsonv_arr_set(Jsonv_Arena *arena, Jsonv_Arr *a, int index, Jsonv_Value v) {
  /*#region*/
  arr_set(arena, a, index, v);
  /*#endregion*/
}

int arr_get(Arr *a, int index, Value *out) {
  /*#region*/
  if (index < 0 || index >= a->length)
    return 0; // Out of bounds
      
  *out = a->items[index];
  return 1;
  /*#endregion*/
}

bool jsonv_arr_get(Jsonv_Arr *a, int index, Jsonv_Value *out) {
  /*#region*/
  return arr_get(a, index, out) != 0;
  /*#endregion*/
}

// Internal testing getters

Jsonv_Value *arr_get_items(const Jsonv_Arr *a) {
  /*#region*/
  return a ? a->items : NULL;
  /*#endregion*/
}

int arr_get_length(const Jsonv_Arr *a) {
  /*#region*/
  return a ? a->length : 0;
  /*#endregion*/
}

int arr_get_capacity(const Jsonv_Arr *a) {
  /*#region*/
  return a ? a->capacity : 0;
  /*#endregion*/
}

int arr_get_refcount(const Jsonv_Arr *a) {
  /*#region*/
  return a ? a->refcount : 0;
  /*#endregion*/
}
