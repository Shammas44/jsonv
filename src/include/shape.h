#ifndef _JSONV_SHAPE_H
#define _JSONV_SHAPE_H

#include "macro.h"
#include "arena.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Namespaced Opaque Types
typedef struct Jsonv_Shape Jsonv_Shape;
typedef struct Jsonv_Obj Jsonv_Obj;
typedef struct Jsonv_Arr Jsonv_Arr;

// Namespaced Value Types
typedef enum {
  JSONV_VAL_UNDEFINED = 0,
  JSONV_VAL_INT = 1,
  JSONV_VAL_PTR = 2,
  JSONV_VAL_DOUBLE = 3,
  JSONV_VAL_OBJ = 4,
  JSONV_VAL_ARRAY = 5,
  JSONV_VAL_NULL = 6,
  JSONV_VAL_BOOLEAN = 7,
  JSONV_VAL_STRING = 8,
  JSONV_VAL_UNRESOLVABLE = 9,
  JSONV_VAL_IMPOSSIBLE = 10
} Jsonv_ValueTag;

typedef struct Jsonv_Value {
  Jsonv_ValueTag tag;
  union {
    int64_t i;
    double d;
    void *p;
    bool boolean;
  } as;
} Jsonv_Value;

// Public Value Constructors & Helpers
JSONV_API Jsonv_Value jsonv_val_undefined(void);
JSONV_API Jsonv_Value jsonv_val_int(int64_t x);
JSONV_API Jsonv_Value jsonv_val_double(double x);
JSONV_API Jsonv_Value jsonv_val_bool(bool x);
JSONV_API Jsonv_Value jsonv_val_null(void);
JSONV_API Jsonv_Value jsonv_val_str(const char *x);
JSONV_API Jsonv_Value jsonv_val_obj(Jsonv_Obj *x);
JSONV_API Jsonv_Value jsonv_val_arr(Jsonv_Arr *x);

JSONV_API size_t jsonv_val_str_len(Jsonv_Value v);

// Public Object Operations
JSONV_API Jsonv_Obj* jsonv_obj_new(Jsonv_Arena *arena, Jsonv_Shape *root);
JSONV_API void jsonv_obj_set(Jsonv_Arena *arena, Jsonv_Obj *o, const char *key, Jsonv_Value v);
JSONV_API bool jsonv_obj_get(Jsonv_Obj *o, const char *key, Jsonv_Value *out);
JSONV_API int jsonv_obj_length(const Jsonv_Obj *o);
JSONV_API const char* jsonv_obj_key_at(const Jsonv_Obj *o, int index);
JSONV_API Jsonv_Value jsonv_obj_val_at(const Jsonv_Obj *o, int index);

// Public Array Operations
JSONV_API Jsonv_Arr* jsonv_arr_new(Jsonv_Arena *arena);
JSONV_API void jsonv_arr_set(Jsonv_Arena *arena, Jsonv_Arr *a, int index, Jsonv_Value v);
JSONV_API bool jsonv_arr_get(Jsonv_Arr *a, int index, Jsonv_Value *out);
JSONV_API int jsonv_arr_length(const Jsonv_Arr *a);
JSONV_API Jsonv_Value jsonv_arr_val_at(const Jsonv_Arr *a, int index);

// Public Shape Operations
JSONV_API Jsonv_Shape* jsonv_shape_root(void);

// Nested format accessor
JSONV_API Jsonv_Value jsonv_value_get_path(Jsonv_Value current, const char *fmt, ...);

#endif
