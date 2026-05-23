#ifndef _JSONV_SHAPE_H_INCLUDED
#define _JSONV_SHAPE_H_INCLUDED
#include <stdint.h>
#include <time.h>
#include <stdbool.h>

/* ------------------- Shape ------------------- */

typedef struct Shape Shape;
typedef struct Transition Transition;

struct Shape {
    Shape* parent;          // previous shape in the chain
    const char* last_key;   // key that was added to parent to form this shape
    int last_slot;          // slot index assigned to last_key
    int slot_count;         // total slots in this shape
    Transition* transitions;// add-property transitions
};

/* Create the empty/root shape */
Shape* shape_root(void);

/* Find an existing transition: shape + key -> next_shape */
Shape* shape_find_transition(Shape* s, const char* key) ;

/* Add a transition entry to a shape */
void shape_add_transition(Shape* from, const char* key, Shape* to);

/*
  "Add property" operation:
  - If shape already has a transition for key, reuse it.
  - Otherwise create a new child shape with one more slot.
*/
Shape* shape_transition_add(Shape* s, const char* key) ;

/* Lookup a key's slot index in a shape by walking parent chain */
int shape_lookup_slot(Shape* s, const char* key) ;

const char* shape_get_key_at(Shape* s, int slot);


#endif
