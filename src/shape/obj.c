#include "obj.h"
#include "value.h"
#include <stdlib.h>

void obj_free(Obj *o) {
  for (int i = 0; i < o->shape->slot_count; i++) {
    value_release(o->slots[i]);
  }
  free(o->slots);
  free(o);
}

/* ------------------- Object ------------------- */

Obj *obj_new(Shape *root) {
  Obj *o = (Obj *)calloc(1, sizeof(Obj));
  o->shape = root;
  o->slots = NULL;
  o->capacity = 0;
  o->refcount = 0;
  return o;
}

void obj_ensure_capacity(Obj *o, int needed) {
  if (o->capacity >= needed)
    return;
  int newcap = o->capacity ? o->capacity : 4;
  while (newcap < needed)
    newcap *= 2;

  o->slots = (Value *)realloc(o->slots, (size_t)newcap * sizeof(Value));
  // initialize new memory to undefined
  for (int i = o->capacity; i < newcap; i++) {
    o->slots[i] = val_undefined();
  }
  o->capacity = newcap;
}

/*
  Set property:
  - If key already exists in current shape: write to slot
  - Else: transition to a new shape that adds the key, grow slot storage, write
*/
void obj_set(Obj *o, const char *key, Value v) {
  int slot = shape_lookup_slot(o->shape, key);

  if (slot >= 0) {
    // Existing property → overwrite
    Value old = o->slots[slot];
    value_retain(v); // Hold new reference first
    o->slots[slot] = v;
    value_release(old); // Release old reference
    return;
  }

  // --- Adding a new property ---

  Shape *newshape = shape_transition_add(o->shape, key);
  o->shape = newshape;

  obj_ensure_capacity(o, o->shape->slot_count);

  int new_slot = o->shape->slot_count - 1;

  // Initialize slot to undefined (safe)
  o->slots[new_slot] = val_undefined();

  value_retain(v); // Retain new value
  o->slots[new_slot] = v;
}

int obj_get(Obj *o, const char *key, Value *out) {
  int slot = shape_lookup_slot(o->shape, key);
  if (slot < 0)
    return 0;
  *out = o->slots[slot];
  return 1;
}
