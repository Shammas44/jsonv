#include "shape.internal.h"
#include "mem.h"
#include <stdlib.h>
#include <string.h>

void obj_free(Obj *o) {
  /*#region*/
  if (o->slots && o->capacity > 0) {
    recycle_val_array(o->slots, o->capacity);
    o->slots = NULL;
    o->capacity = 0;
  }
  o->slots = (Value *)obj_free_list;
  obj_free_list = o;
  /*#endregion*/
}

/* ------------------- Object ------------------- */

Obj *obj_new(Jsonv_Arena *arena, Shape *root) {
  /*#region*/
  Obj *o;
  if (obj_free_list) {
    o = obj_free_list;
    obj_free_list = (Obj *)o->slots;
  } else {
    o = (Obj *)jsonv_arena_alloc(arena, sizeof(Obj));
    if (!o) return NULL;
  }
  o->shape = root;
  o->slots = NULL;
  o->capacity = 0;
  o->refcount = 0;
  return o;
  /*#endregion*/
}

Jsonv_Obj *jsonv_obj_new(Jsonv_Arena *arena, Jsonv_Shape *root) {
  /*#region*/
  return obj_new(arena, root);
  /*#endregion*/
}

void obj_ensure_capacity(Jsonv_Arena *arena, Obj *o, int needed) {
  /*#region*/
  if (o->capacity >= needed)
    return;
  int newcap = o->capacity ? o->capacity : 4;
  while (newcap < needed)
    newcap *= 2;

  Value *new_slots = allocate_val_array(arena, newcap);
  if (!new_slots) return;

  // Copy old slots
  if (o->slots && o->capacity > 0) {
    memcpy(new_slots, o->slots, (size_t)o->capacity * sizeof(Value));
    recycle_val_array(o->slots, o->capacity);
  }

  // Initialize new slots to undefined
  for (int i = o->capacity; i < newcap; i++) {
    new_slots[i] = jsonv_val_undefined();
  }

  o->slots = new_slots;
  o->capacity = newcap;
  /*#endregion*/
}

void obj_set(Jsonv_Arena *arena, Obj *o, const char *key, Value v) {
  /*#region*/
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

  Shape *newshape = shape_transition_add(arena, o->shape, key);
  if (!newshape) return;
  o->shape = newshape;

  obj_ensure_capacity(arena, o, o->shape->slot_count);

  int new_slot = o->shape->slot_count - 1;

  // Initialize slot to undefined (safe)
  o->slots[new_slot] = jsonv_val_undefined();

  value_retain(v); // Retain new value
  o->slots[new_slot] = v;
  /*#endregion*/
}

void jsonv_obj_set(Jsonv_Arena *arena, Jsonv_Obj *o, const char *key, Jsonv_Value v) {
  /*#region*/
  obj_set(arena, o, key, v);
  /*#endregion*/
}

int obj_get(Obj *o, const char *key, Value *out) {
  /*#region*/
  int slot = shape_lookup_slot(o->shape, key);
  if (slot < 0)
    return 0;
  *out = o->slots[slot];
  return 1;
  /*#endregion*/
}

bool jsonv_obj_get(Jsonv_Obj *o, const char *key, Jsonv_Value *out) {
  /*#region*/
  return obj_get(o, key, out) != 0;
  /*#endregion*/
}

// Internal testing getters

Jsonv_Shape *obj_get_shape(const Jsonv_Obj *o) {
  /*#region*/
  return o ? o->shape : NULL;
  /*#endregion*/
}

Jsonv_Value *obj_get_slots(const Jsonv_Obj *o) {
  /*#region*/
  return o ? o->slots : NULL;
  /*#endregion*/
}

int obj_get_capacity(const Jsonv_Obj *o) {
  /*#region*/
  return o ? o->capacity : 0;
  /*#endregion*/
}

int obj_get_refcount(const Jsonv_Obj *o) {
  /*#region*/
  return o ? o->refcount : 0;
  /*#endregion*/
}
