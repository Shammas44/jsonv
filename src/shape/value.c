#include "value.h"
#include <stdio.h>
#include <stdlib.h>
/* ------------------- Value ------------------- */

extern void obj_free(void *);
extern void arr_free(void *);

typedef struct {
  int refcount;
} Ref;

Value val_undefined(void) {
  Value v;
  v.tag = VAL_UNDEFINED;
  v.as.p = NULL;
  return v;
}

Value val_int(int64_t x) {
  Value v;
  v.tag = VAL_INT;
  v.as.i = x;
  return v;
}

Value val_double(double x) {
  Value v;
  v.tag = VAL_DOUBLE;
  v.as.d = x;
  return v;
}

Value val_obj(void *x) {
  Value v;
  v.tag = VAL_OBJ;
  v.as.p = x;
  return v;
}

Value val_arr(void *x) {
  Value v;
  v.tag = VAL_ARRAY;
  v.as.p = x;
  return v;
}

Value val_bool(bool x) {
  Value v;
  v.tag = VAL_BOOLEAN;
  v.as.boolean = x;
  return v;
}

Value val_str(void *x) {
  Value v;
  v.tag = VAL_STRING;
  v.as.p = x;
  return v;
}

Value val_null(void) {
  Value v;
  v.tag = VAL_NULL;
  return v;
}

Value val_impossible(void){
  Value v;
  v.tag = VAL_IMPOSSIBLE;
  return v;
}

Value val_unresolvable(void){
  Value v;
  v.tag = VAL_UNRESOLVABLE;
  return v;
}

void value_retain(Value v) {
  if (v.tag == VAL_OBJ || v.tag == VAL_ARRAY)
    ((Ref *)(v.as.p))->refcount++;
}

void value_release(Value v) {
  if (v.tag == VAL_OBJ) {
    if (--((Ref *)(v.as.p))->refcount == 0) {
      obj_free(v.as.p);
    }
  } else if (v.tag == VAL_ARRAY) {
    if (--((Ref *)(v.as.p))->refcount == 0) {
      arr_free(v.as.p);
    }
  }
}
