#include "shape.internal.h"
#include "mem.h"
#include <stdlib.h>
#include <string.h>

void arr_free(Arr *a) {
  /*#region*/
  if (a->items && a->capacity > 0) {
    recycle_val_array(a->items, a->capacity);
    a->items = NULL;
    a->capacity = 0;
    a->length = 0;
  }
  a->items = (Value *)arr_free_list;
  arr_free_list = a;
  /*#endregion*/
}

/* ------------------- Array ------------------- */

Arr *arr_new(Jsonv_Arena *arena) {
  /*#region*/
  Arr *a;
  if (arr_free_list) {
    a = arr_free_list;
    arr_free_list = (Arr *)a->items;
  } else {
    a = (Arr *)jsonv_arena_alloc(arena, sizeof(Arr));
    if (!a) return NULL;
  }
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

  Value *new_items = allocate_val_array(arena, newcap);
  if (!new_items) return;
  
  // Copy old items
  if (a->items && a->capacity > 0) {
    memcpy(new_items, a->items, (size_t)a->capacity * sizeof(Value));
    recycle_val_array(a->items, a->capacity);
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
