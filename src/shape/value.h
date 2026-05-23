#ifndef VALUE_H
#define VALUE_H
#include <stdbool.h>
#include <stdint.h>

typedef enum {
  VAL_UNDEFINED = 0,
  VAL_INT = 1,
  VAL_PTR = 2,
  VAL_DOUBLE,
  VAL_OBJ,
  VAL_ARRAY,
  VAL_NULL,
  VAL_BOOLEAN,
  VAL_STRING,
  VAL_UNRESOLVABLE,
  VAL_IMPOSSIBLE,
} ValueTag;

typedef struct Value Value;

typedef struct Value {
  ValueTag tag;
  union {
    int64_t i;
    double d;
    void *p;
    bool boolean;
  } as;
} Value;

Value val_undefined(void);

Value val_int(int64_t x);

Value val_obj(void *x);

Value val_arr(void *x);

Value val_bool(bool x);

Value val_double(double x);

Value val_arr(void *x);

Value val_str(void *x);

Value val_null(void);

Value val_impossible(void);

Value val_unresolvable(void);

void value_retain(Value v);

void value_release(Value v);


#endif
