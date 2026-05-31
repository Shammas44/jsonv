#ifndef _JSONV_VALUE_H
#define _JSONV_VALUE_H
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

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

typedef struct {
  uint32_t length;
  char data[];
} StringHeader;

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

typedef char *lstr_t;
typedef const char *const_lstr_t;

Value val_str(const_lstr_t x);

Value val_null(void);

Value val_impossible(void);

Value val_unresolvable(void);

void value_retain(Value v);

void value_release(Value v);

static inline size_t val_str_len(Value v) {
  if (v.tag == VAL_STRING && v.as.p) {
    return ((StringHeader *)v.as.p - 1)->length;
  }
  return 0;
}

#endif
