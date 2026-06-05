#include "shape.internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <assert.h>

/* Helper to get length of const_lstr_t in O(1) */
static inline size_t lstr_len(const char *s) {
  /*#region*/
  if (!s) return 0;
  return ((const StringHeader *)s - 1)->length;
  /*#endregion*/
}

static Jsonv_Arena *global_shape_arena = NULL;

static void init_global_shape_arena(void) {
  /*#region*/
  if (!global_shape_arena) {
    global_shape_arena = jsonv_arena_new(4096, 10 * 1024 * 1024, 1024 * 1024);
  }
  /*#endregion*/
}

void jsonv_shape_clear_global_arena(void) {
  /*#region*/
  if (global_shape_arena) {
    jsonv_arena_destroy(global_shape_arena);
    global_shape_arena = NULL;
  }
  /*#endregion*/
}

/* ---------------- Transition ----------------- */

static void add_transition(Shape* from, const char *key, Shape* to) {
  /*#region*/
  init_global_shape_arena();
  // Conforms strictly to memory laws: Allocates on Arena, no standard malloc/calloc/free
  Transition* t = (Transition*)jsonv_arena_alloc(global_shape_arena, sizeof(Transition));
  if (!t) return;
  t->key = key;
  t->next_shape = to;
  t->next = from->transitions;
  from->transitions = t;
  /*#endregion*/
}

Shape* shape_transition_add(Shape* s, const char *key) {
  /*#region*/
  init_global_shape_arena();
  Shape* existing = shape_find_transition(s, key);
  if (existing) return existing;

  Shape* child = (Shape*)jsonv_arena_alloc(global_shape_arena, sizeof(Shape));
  if (!child) return NULL;
  child->parent = s;
  child->last_key = key;
  child->last_slot = s->slot_count;  // Assign next slot
  child->slot_count = s->slot_count + 1;
  child->transitions = NULL;

  add_transition(s, key, child);
  return child;
  /*#endregion*/
}

Shape* shape_find_transition(Shape* s, const char *key) {
  /*#region*/
  size_t key_len = lstr_len(key);
  for (Transition* t = s->transitions; t; t = t->next) {
    size_t t_len = lstr_len(t->key);
    if (t_len == key_len && memcmp(t->key, key, key_len) == 0) {
      return t->next_shape;
    }
  }
  return NULL;
  /*#endregion*/
}

/* ------------------- Shape ------------------- */

Shape* jsonv_shape_root(void) {
  /*#region*/
  init_global_shape_arena();
  Shape* s = (Shape*)jsonv_arena_alloc(global_shape_arena, sizeof(Shape));
  if (!s) return NULL;
  s->parent = NULL;
  s->last_key = NULL;
  s->last_slot = -1;
  s->slot_count = 0;
  s->transitions = NULL;
  return s;
  /*#endregion*/
}

int shape_lookup_slot(Shape* s, const char *key) {
  /*#region*/
  size_t key_len = lstr_len(key);
  for (Shape* cur = s; cur && cur->last_key; cur = cur->parent) {
    size_t cur_len = lstr_len(cur->last_key);
    if (cur_len == key_len && memcmp(cur->last_key, key, key_len) == 0) {
      return cur->last_slot;
    }
  }
  return -1;
  /*#endregion*/
}

const char* shape_get_key_at(Shape* s, int slot) {
  /*#region*/
  while (s) {
    if (s->last_slot == slot)
      return s->last_key;
    s = s->parent;
  }
  return "?";
  /*#endregion*/
}

// Internal testing getters (not exported by public dynamic interfaces)

Jsonv_Shape *shape_get_parent(const Jsonv_Shape *s) {
  /*#region*/
  return s ? s->parent : NULL;
  /*#endregion*/
}

int shape_get_slot_count(const Jsonv_Shape *s) {
  /*#region*/
  return s ? s->slot_count : 0;
  /*#endregion*/
}

int shape_get_last_slot(const Jsonv_Shape *s) {
  /*#region*/
  return s ? s->last_slot : -1;
  /*#endregion*/
}

const char *shape_get_last_key(const Jsonv_Shape *s) {
  /*#region*/
  return s ? s->last_key : NULL;
  /*#endregion*/
}
