#ifndef _JSONV_SHAPE_H
#define _JSONV_SHAPE_H
#include "value.h" // For lstr_t and const_lstr_t
#include "arena.h"
#include <stdbool.h>

/* ------------------- Shape ------------------- */

typedef struct Shape Shape;
typedef struct Transition Transition;

struct Transition {
    const_lstr_t key;       // length-prefixed property name
    Shape*      next_shape; // shape after adding key
    Transition* next;       // linked list
};

struct Shape {
    Shape* parent;          // previous shape in the chain
    const_lstr_t last_key;  // key that was added to parent to form this shape
    int last_slot;          // slot index assigned to last_key
    int slot_count;         // total slots in this shape
    Transition* transitions;// add-property transitions
};

/* Create the empty/root shape inside the Arena memory pool */
Shape* shape_root(Jsonv_Arena *arena);

/* Find an existing transition: shape + key -> next_shape */
Shape* shape_find_transition(Shape* s, const_lstr_t key);

/* Add a transition entry to a shape inside the Arena pool */
void shape_add_transition(Jsonv_Arena *arena, Shape* from, const_lstr_t key, Shape* to);

/*
  "Add property" operation:
  - If shape already has a transition for key, reuse it.
  - Otherwise create a new child shape inside the Arena with one more slot.
*/
Shape* shape_transition_add(Jsonv_Arena *arena, Shape* s, const_lstr_t key);

/* Lookup a key's slot index in a shape by walking parent chain (O(1) len checks + memcmp) */
int shape_lookup_slot(Shape* s, const_lstr_t key);

/* Retrieve the key at a specific slot by walking parent chain */
const_lstr_t shape_get_key_at(Shape* s, int slot);

#endif
