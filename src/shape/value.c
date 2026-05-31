#include "shape.internal.h"
#include <stdio.h>
#include <stdlib.h>

/* ------------------- Value ------------------- */

typedef struct {
  int refcount;
} Ref;

Value val_undefined(void) {
  /*#region*/
  Value v;
  v.tag = VAL_UNDEFINED;
  v.as.p = NULL;
  return v;
  /*#endregion*/
}

Jsonv_Value jsonv_val_undefined(void) {
  /*#region*/
  return val_undefined();
  /*#endregion*/
}

Value val_int(int64_t x) {
  /*#region*/
  Value v;
  v.tag = VAL_INT;
  v.as.i = x;
  return v;
  /*#endregion*/
}

Jsonv_Value jsonv_val_int(int64_t x) {
  /*#region*/
  return val_int(x);
  /*#endregion*/
}

Value val_double(double x) {
  /*#region*/
  Value v;
  v.tag = VAL_DOUBLE;
  v.as.d = x;
  return v;
  /*#endregion*/
}

Jsonv_Value jsonv_val_double(double x) {
  /*#region*/
  return val_double(x);
  /*#endregion*/
}

Value val_obj(void *x) {
  /*#region*/
  Value v;
  v.tag = VAL_OBJ;
  v.as.p = x;
  return v;
  /*#endregion*/
}

Jsonv_Value jsonv_val_obj(Jsonv_Obj *x) {
  /*#region*/
  return val_obj(x);
  /*#endregion*/
}

Value val_arr(void *x) {
  /*#region*/
  Value v;
  v.tag = VAL_ARRAY;
  v.as.p = x;
  return v;
  /*#endregion*/
}

Jsonv_Value jsonv_val_arr(Jsonv_Arr *x) {
  /*#region*/
  return val_arr(x);
  /*#endregion*/
}

Value val_bool(bool x) {
  /*#region*/
  Value v;
  v.tag = VAL_BOOLEAN;
  v.as.boolean = x;
  return v;
  /*#endregion*/
}

Jsonv_Value jsonv_val_bool(bool x) {
  /*#region*/
  return val_bool(x);
  /*#endregion*/
}

Value val_str(const char *x) {
  /*#region*/
  Value v;
  v.tag = VAL_STRING;
  v.as.p = (void *)x;
  return v;
  /*#endregion*/
}

Jsonv_Value jsonv_val_str(const char *x) {
  /*#region*/
  return val_str(x);
  /*#endregion*/
}

Value val_null(void) {
  /*#region*/
  Value v;
  v.tag = VAL_NULL;
  v.as.p = NULL;
  return v;
  /*#endregion*/
}

Jsonv_Value jsonv_val_null(void) {
  /*#region*/
  return val_null();
  /*#endregion*/
}

Value val_impossible(void) {
  /*#region*/
  Value v;
  v.tag = VAL_IMPOSSIBLE;
  v.as.p = NULL;
  return v;
  /*#endregion*/
}

Value val_unresolvable(void) {
  /*#region*/
  Value v;
  v.tag = VAL_UNRESOLVABLE;
  v.as.p = NULL;
  return v;
  /*#endregion*/
}

void value_retain(Value v) {
  /*#region*/
  if (v.tag == VAL_OBJ || v.tag == VAL_ARRAY) {
    if (v.as.p) {
      ((Ref *)(v.as.p))->refcount++;
    }
  }
  /*#endregion*/
}

void value_release(Value v) {
  /*#region*/
  if (v.tag == VAL_OBJ) {
    if (v.as.p) {
      if (--((Ref *)(v.as.p))->refcount == 0) {
        obj_free(v.as.p);
      }
    }
  } else if (v.tag == VAL_ARRAY) {
    if (v.as.p) {
      if (--((Ref *)(v.as.p))->refcount == 0) {
        arr_free(v.as.p);
      }
    }
  }
  /*#endregion*/
}

size_t jsonv_val_str_len(Jsonv_Value v) {
  /*#region*/
  if (v.tag == JSONV_VAL_STRING && v.as.p) {
    return ((StringHeader *)v.as.p - 1)->length;
  }
  return 0;
  /*#endregion*/
}
