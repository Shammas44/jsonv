#ifndef _JSONV_OBJ_H_INCLUDED
#define _JSONV_OBJ_H_INCLUDED
#include "shape.h"
#include "value.h"
#include "arena.h"

typedef struct {
  int refcount; // MUST be the very first field!
  Shape *shape;
  Value *slots; // array of length shape->slot_count
  int capacity; // allocated slot capacity
} Obj;

/* ------------------- Object ------------------- */

/* Instantiate a new dynamic Object inside the Arena pool */
Obj *obj_new(Arena *arena, Shape *root);

/* 
  Release resources. On Arena systems, memory is automatically reclaimed,
  so this is kept for legacy compatibility but is standard-allocation free.
*/
void obj_free(Obj *o);

/* Ensure slot array capacity using Arena allocations */
void obj_ensure_capacity(Arena *arena, Obj *o, int needed);

/*
  Set property:
  - If key already exists in current shape: write to slot
  - Else: transition to a new shape inside the Arena, grow slot storage contiguously in the Arena, write
*/
void obj_set(Arena *arena, Obj *o, const_lstr_t key, Value v);

/* Get property value using fast-path O(1) shape slot lookups */
int obj_get(Obj *o, const_lstr_t key, Value *out);

#endif
