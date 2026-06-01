#ifndef _JSONV_SHAPE_INTERNAL_H
#define _JSONV_SHAPE_INTERNAL_H

#include "shape.h"

// Define concrete internal structure layout
typedef struct Transition Transition;
struct Transition {
    const char* key;
    Jsonv_Shape* next_shape;
    Transition* next;
};

struct Jsonv_Shape {
    Jsonv_Shape* parent;
    const char* last_key;
    int last_slot;
    int slot_count;
    Transition* transitions;
};

struct Jsonv_Obj {
    int refcount; // Must remain first field
    Jsonv_Shape *shape;
    Jsonv_Value *slots;
    int capacity;
};

struct Jsonv_Arr {
    int refcount; // Must remain first field
    Jsonv_Value *items;
    int length;
    int capacity;
};

typedef struct {
  uint32_t length;
  char data[];
} StringHeader;

// Local private aliases to maintain clean internal code representation
typedef Jsonv_Value Value;
typedef Jsonv_ValueTag ValueTag;
typedef Jsonv_Shape Shape;
typedef Jsonv_Obj Obj;
typedef Jsonv_Arr Arr;
typedef char *lstr_t;
typedef const char *const_lstr_t;

static inline size_t val_str_len(Jsonv_Value v) {
  if (v.tag == JSONV_VAL_STRING && v.as.p) {
    return ((StringHeader *)v.as.p - 1)->length;
  }
  return 0;
}

#define VAL_UNDEFINED JSONV_VAL_UNDEFINED
#define VAL_INT JSONV_VAL_INT
#define VAL_PTR JSONV_VAL_PTR
#define VAL_DOUBLE JSONV_VAL_DOUBLE
#define VAL_OBJ JSONV_VAL_OBJ
#define VAL_ARRAY JSONV_VAL_ARRAY
#define VAL_NULL JSONV_VAL_NULL
#define VAL_BOOLEAN JSONV_VAL_BOOLEAN
#define VAL_STRING JSONV_VAL_STRING
#define VAL_UNRESOLVABLE JSONV_VAL_UNRESOLVABLE
#define VAL_IMPOSSIBLE JSONV_VAL_IMPOSSIBLE

// Internal-only functions
void jsonv_shape_clear_global_arena(void);
Shape* shape_root(Jsonv_Arena *arena);
Shape* shape_find_transition(Shape* s, const char* key);
void shape_add_transition(Jsonv_Arena *arena, Shape* from, const char* key, Shape* to);
Shape* shape_transition_add(Jsonv_Arena *arena, Shape* s, const char* key);
int shape_lookup_slot(Shape* s, const char* key);
const char* shape_get_key_at(Shape* s, int slot);

Obj *obj_new(Jsonv_Arena *arena, Shape *root);
void obj_free(Obj *o);
void obj_set(Jsonv_Arena *arena, Obj *o, const char *key, Value v);
int obj_get(Obj *o, const char *key, Value *out);
void obj_ensure_capacity(Jsonv_Arena *arena, Obj *o, int needed);

Arr *arr_new(Jsonv_Arena *arena);
void arr_free(Arr *a);
void arr_set(Jsonv_Arena *arena, Arr *a, int index, Value v);
int arr_get(Arr *a, int index, Value *out);
void arr_ensure_capacity(Jsonv_Arena *arena, Arr *a, int needed);

Value val_undefined(void);
Value val_int(int64_t x);
Value val_double(double x);
Value val_bool(bool x);
Value val_null(void);
Value val_str(const char *x);
Value val_obj(void *x);
Value val_arr(void *x);
Value val_impossible(void);
Value val_unresolvable(void);

void value_retain(Jsonv_Value v);
void value_release(Jsonv_Value v);

void print_value(Value v);
void print_shape(Shape *s);

// Expose internal testing getter helper functions (not marked with JSONV_API)
Jsonv_Shape *shape_get_parent(const Jsonv_Shape *s);
int shape_get_slot_count(const Jsonv_Shape *s);
int shape_get_last_slot(const Jsonv_Shape *s);
const char *shape_get_last_key(const Jsonv_Shape *s);

Jsonv_Shape *obj_get_shape(const Jsonv_Obj *o);
Jsonv_Value *obj_get_slots(const Jsonv_Obj *o);
int obj_get_capacity(const Jsonv_Obj *o);
int obj_get_refcount(const Jsonv_Obj *o);

Jsonv_Value *arr_get_items(const Jsonv_Arr *a);
int arr_get_length(const Jsonv_Arr *a);
int arr_get_capacity(const Jsonv_Arr *a);
int arr_get_refcount(const Jsonv_Arr *a);

#endif
