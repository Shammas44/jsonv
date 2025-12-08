#include "list.h"
#include "assert.h"
#include "mem.h"
#include <stdarg.h>
#include <stddef.h>

#define T List

T list_new(void *x, ...) {
  /*#region*/
  va_list ap;
  T list, *p = &list;
  va_start(ap, x);
  for (; x; x = va_arg(ap, void *)) {
    NEW(*p);
    (*p)->first = x;
    p = &(*p)->rest;
  }
  *p = NULL;
  va_end(ap);
  return list;
  /*#endregion*/
}

T list_push(T list, void *x) {
  /*#region*/
  T p;
  NEW(p);
  p->first = x;
  p->rest = list;
  return p;
  /*#endregion*/
}

T list_append(T list, T tail) {
  /*#region*/
  T *p = &list;
  while (*p)
    p = &(*p)->rest;
  *p = tail;
  return list;
  /*#endregion*/
}

T list_copy(T list) {
  /*#region*/
  T head, *p = &head;
  for (; list; list = list->rest) {
    NEW(*p);
    (*p)->first = list->first;
    p = &(*p)->rest;
  }
  *p = NULL;
  return head;
  /*#endregion*/
}

T list_pop(T list, void **x) {
  /*#region*/
  if (list) {
    T head = list->rest;
    if (x)
      *x = list->first;
    FREE(list);
    return head;
  } else
    return list;
  /*#endregion*/
}

T list_reverse(T list) {
  /*#region*/
  T head = NULL, next;
  for (; list; list = next) {
    next = list->rest;
    list->rest = head;
    head = list;
  }
  return head;
  /*#endregion*/
}

int list_length(T list) {
  /*#region*/
  int n;
  for (n = 0; list; list = list->rest)
    n++;
  return n;
  /*#endregion*/
}

void list_free(T *list) {
  /*#region*/
  T next;
  assert(list);
  for (; *list; *list = next) {
    next = (*list)->rest;
    FREE(*list);
  }
  /*#endregion*/
}

void list_map(T list, void apply(void **x, void *cl), void *cl) {
  /*#region*/
  assert(apply);
  for (; list; list = list->rest)
    apply(&list->first, cl);
  /*#endregion*/
}

void **list_to_array(T list, void *end) {
  /*#region*/
  int i, n = list_length(list);
  void **array = ALLOC((n + 1) * sizeof(*array));
  for (i = 0; i < n; i++) {
    array[i] = list->first;
    list = list->rest;
  }
  array[i] = end;
  return array;
  /*#endregion*/
}
