#ifndef _JSONV_ARR_H_INCLUDED
#define _JSONV_ARR_H_INCLUDED
#include "value.h"

typedef struct {
  int refcount; // MUST be the very first field! 
  Value *items;
  int length;
  int capacity;
} Arr;

Arr *arr_new(void);
void arr_free(Arr *a);
void arr_ensure_capacity(Arr *a, int needed);
void arr_set(Arr *a, int index, Value v);
int arr_get(Arr *a, int index, Value *out);

#endif
