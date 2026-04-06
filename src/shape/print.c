#include "value.h"
#include "shape.h"
#include "obj.h"
#include "arr.h"
#include <stdio.h>
#include <stdlib.h>

typedef struct Visited {
  const void *ptr;
  struct Visited *next;
} Visited;

static void print_object(Obj *o, Visited *visited);
static void print_array(Arr *a, Visited *visited);
static void print_value_internal(Value v, Visited *visited);

static int visited_contains(Visited *v, const void *ptr) {
  while (v) {
    if (v->ptr == ptr)
      return 1;
    v = v->next;
  }
  return 0;
}

static Visited *visited_push(Visited *v, const void *ptr) {
  Visited *node = malloc(sizeof(Visited));
  node->ptr = ptr;
  node->next = v;
  return node;
}

void print_value(Value v) { print_value_internal(v, NULL); }

static void print_value_internal(Value v, Visited *visited) {
  switch (v.tag) {

  case VAL_UNDEFINED:
    printf("undefined");
    break;

  case VAL_INT:
    printf("%lld", (long long)v.as.i);
    break;

  case VAL_DOUBLE:
    printf("%f", v.as.d);
    break;

  case VAL_OBJ:
    print_object(v.as.p, visited);
    break;

  case VAL_BOOLEAN:
    printf("%s", v.as.boolean ? "true": "false");
    break;

  case VAL_NULL:
    printf("null");
    break;

  case VAL_ARRAY:
    print_array(v.as.p, visited);
    break;

  case VAL_STRING:
    printf("%s",(char*)v.as.p);
    break;

  default:
    printf("<?>");
    break;
  }
}

static void print_object(Obj *o, Visited *visited) {
  if (visited_contains(visited, o)) {
    printf("{<cycle>}");
    return;
  }

  visited = visited_push(visited, o);

  printf("{");

  int first = 1;

  for (int i = 0; i < o->shape->slot_count; i++) {
    const char *key = shape_get_key_at(o->shape, i); // you must implement this
    Value v = o->slots[i];

    if (!first)
      printf(", ");
    first = 0;

    printf("\"%s\": ", key);
    print_value_internal(v, visited);
  }

  printf("}");
}

static void print_array(Arr *a, Visited *visited) {
  if (visited_contains(visited, a)) {
    printf("[<cycle>]");
    return;
  }

  visited = visited_push(visited, a);

  printf("[");

  for (int i = 0; i < a->length; i++) {
    if (i > 0)
      printf(", ");
    print_value_internal(a->items[i], visited);
  }

  printf("]");
}

void print_shape(Shape *s) {
  // #region
  printf("Shape@%p slots=%d\n", (void *)s, s->slot_count);
  for (Shape *cur = s; cur && cur->last_key; cur = cur->parent) {
    printf("  key '%s' -> slot %d\n", cur->last_key, cur->last_slot);
  }
  // #endregion
}
