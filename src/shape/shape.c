#include "shape.h"
#include "mem.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <assert.h>

/* Helper to get length of const_lstr_t in O(1) */
static inline size_t lstr_len(const_lstr_t s) {
  /*#region*/
  if (!s) return 0;
  return ((const StringHeader *)s - 1)->length;
  /*#endregion*/
}

/* ---------------- Transition ----------------- */

Shape* shape_find_transition(Shape* s, const_lstr_t key) {
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

void shape_add_transition(Jsonv_Arena *arena, Shape* from, const_lstr_t key, Shape* to) {
  /*#region*/
  // Conforms strictly to memory laws: Allocates on Arena, no standard malloc/calloc/free
  Transition* t = (Transition*)jsonv_arena_alloc(arena, sizeof(Transition));
  if (!t) return;
  t->key = key;
  t->next_shape = to;
  t->next = from->transitions;
  from->transitions = t;
  /*#endregion*/
}

Shape* shape_transition_add(Jsonv_Arena *arena, Shape* s, const_lstr_t key) {
  /*#region*/
  Shape* existing = shape_find_transition(s, key);
  if (existing) return existing;

  Shape* child = (Shape*)jsonv_arena_alloc(arena, sizeof(Shape));
  if (!child) return NULL;
  child->parent = s;
  child->last_key = key;
  child->last_slot = s->slot_count;  // Assign next slot
  child->slot_count = s->slot_count + 1;
  child->transitions = NULL;

  shape_add_transition(arena, s, key, child);
  return child;
  /*#endregion*/
}

/* ------------------- Shape ------------------- */

Shape* shape_root(Jsonv_Arena *arena) {
  /*#region*/
  Shape* s = (Shape*)jsonv_arena_alloc(arena, sizeof(Shape));
  if (!s) return NULL;
  s->parent = NULL;
  s->last_key = NULL;
  s->last_slot = -1;
  s->slot_count = 0;
  s->transitions = NULL;
  return s;
  /*#endregion*/
}

int shape_lookup_slot(Shape* s, const_lstr_t key) {
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

const_lstr_t shape_get_key_at(Shape* s, int slot) {
  /*#region*/
  while (s) {
    if (s->last_slot == slot)
      return s->last_key;
    s = s->parent;
  }
  return (const_lstr_t)"?";
  /*#endregion*/
}
