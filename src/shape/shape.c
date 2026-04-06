#include "shape.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <assert.h>

/* ---------------- Transition ----------------- */

struct Transition {
    const char* key;        // property name
    Shape*      next_shape; // shape after adding key
    Transition* next;       // linked list
};

/* Find an existing transition: shape + key -> next_shape */
Shape* shape_find_transition(Shape* s, const char* key) {
    for (Transition* t = s->transitions; t; t = t->next) {
        if (strcmp(t->key, key) == 0) return t->next_shape;
    }
    return NULL;
}

/* Add a transition entry to a shape */
void shape_add_transition(Shape* from, const char* key, Shape* to) {
    Transition* t = (Transition*)calloc(1, sizeof(Transition));
    t->key = key;               // NOTE: assumes key lifetime is stable
    t->next_shape = to;
    t->next = from->transitions;
    from->transitions = t;
}

/*
  "Add property" operation:
  - If shape already has a transition for key, reuse it.
  - Otherwise create a new child shape with one more slot.
*/
Shape* shape_transition_add(Shape* s, const char* key) {
    Shape* existing = shape_find_transition(s, key);
    if (existing) return existing;

    Shape* child = (Shape*)calloc(1, sizeof(Shape));
    child->parent = s;
    child->last_key = key;
    child->last_slot = s->slot_count;  // assign next slot
    child->slot_count = s->slot_count + 1;
    child->transitions = NULL;

    shape_add_transition(s, key, child);
    return child;
}

/* ------------------- Shape ------------------- */

/* Create the empty/root shape */
Shape* shape_root(void) {
    Shape* s = (Shape*)calloc(1, sizeof(Shape));
    s->parent = NULL;
    s->last_key = NULL;
    s->last_slot = -1;
    s->slot_count = 0;
    s->transitions = NULL;
    return s;
}

/* Lookup a key's slot index in a shape by walking parent chain */
int shape_lookup_slot(Shape* s, const char* key) {
    for (Shape* cur = s; cur && cur->last_key; cur = cur->parent) {
        if (strcmp(cur->last_key, key) == 0) {
            return cur->last_slot;
        }
    }
    return -1;
}

const char* shape_get_key_at(Shape* s, int slot) {
    while (s) {
        if (s->last_slot == slot)
            return s->last_key;
        s = s->parent;
    }
    return "?";
}
