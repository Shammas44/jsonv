#include "slidingwindow.h" // Assuming your code is in this header
#include "assert.h"
#include <stdbool.h>
#include <string.h>
#define T Jsonv_SlidingWindow

void jsonv_sw_new(T *w) {
  /*#region*/
  w->head = 0;
  w->count = 0;
  memset(w->data, 0, sizeof(Token) * WINDOW_SIZE);
  /*#endregion*/
}

// Add a new value, sliding the window if it's full
void jsonv_sw_push(T *w, Token value) {
  w->data[w->head] = value;

  // Circular increment of the head
  w->head = (w->head + 1) % WINDOW_SIZE;

  // Increment count until we reach the maximum window size
  if (w->count < WINDOW_SIZE) {
    w->count++;
  }
}

/**
 * Retrieves an element relative to the head.
 * index  0: The most recently added element
 * index -1: The element added before that
 * index -n: Older elements...
 */
Token jsonv_sw_get_by_order(T *w, int index) {
  /*#region*/
  // 1. Ensure the index isn't asking for more than we have
  // (e.g., asking for -10 when we've only added 5 elements)
  int absIndex = (index < 0) ? -index : index;
  assert(absIndex < w->count);

  // 2. Logic: Most recent is at (w->head - 1)
  // We subtract the absolute index to go "backwards" in time
  int physicalIdx = (w->head - 1 - absIndex);

  // 3. Handle the wrap-around for negative results
  while (physicalIdx < 0) {
    physicalIdx += WINDOW_SIZE;
  }

  return w->data[physicalIdx];
  /*#endregion*/
}

int jsonv_sw_count(T *w) {
  /*#region*/
  assert(w);
  return w->count;
  /*#endregion*/
}
