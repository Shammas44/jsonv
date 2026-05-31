#ifndef _JSONV_ARR_H
#define _JSONV_ARR_H
#include "value.h"
#include "arena.h"

typedef struct {
  int refcount; // MUST be the very first field! 
  Value *items;
  int length;
  int capacity;
} Arr;

/* ------------------- Array ------------------- */

/* Instantiate a new dynamic Array inside the Arena pool */
Arr *arr_new(Jsonv_Arena *arena);

/* Clean resources. On Arena systems, this acts as a safe no-op */
void arr_free(Arr *a);

/* Ensure dynamic array capacity contiguously inside the Arena */
void arr_ensure_capacity(Jsonv_Arena *arena, Arr *a, int needed);

/* Set value at index inside the dynamic array, growing in the Arena if needed */
void arr_set(Jsonv_Arena *arena, Arr *a, int index, Value v);

/* Get value at index */
int arr_get(Arr *a, int index, Value *out);

#endif
