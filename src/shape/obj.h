#ifndef _JSONV_OBJ_H_INCLUDED
#define _JSONV_OBJ_H_INCLUDED
#include "shape.h"
#include "value.h"

typedef struct {
  int refcount; // MUST be the very first field! 
  Shape *shape;
  Value *slots; // array of length shape->slot_count
  int capacity; // allocated slot capacity
} Obj;

/* ------------------- Object ------------------- */

Obj *obj_new(Shape *root);
void obj_free(Obj *o);

void obj_ensure_capacity(Obj *o, int needed);

/*
  Set property:
  - If key already exists in current shape: write to slot
  - Else: transition to a new shape that adds the key, grow slot storage, write
*/
void obj_set(Obj *o, const char *key, Value v);

int obj_get(Obj *o, const char *key, Value *out);

#endif
