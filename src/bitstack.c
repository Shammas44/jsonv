#include "bitstack.h"
#include <stdint.h>
#define T Jsonv_BitStack

#define ARRAY_SIZE ((MAX_BITS + 7) / 8)

typedef struct T {
  uint8_t bytes[ARRAY_SIZE];
  int top; // Represents the bit index (0 to MAX_BITS - 1)
} BitStack;

void jsonv_bs_init(T *s) {
  /*#region*/
  s->top = -1;
  for (int i = 0; i < ARRAY_SIZE; i++)
    s->bytes[i] = 0;
  /*#endregion*/
}

int jsonv_bs_top(T *s) {
  /*#region*/
  if (jsonv_bs_is_empty(s))
    return -1;
  return s->bytes[s->top];
  /*#endregion*/
}

bool jsonv_bs_is_full(T *s) {
  /*#region*/
  return s->top == MAX_BITS - 1;
  /*#endregion*/
}

bool jsonv_bs_is_empty(T *s) {
  /*#region*/
  return s->top == -1;
  /*#endregion*/
}

int jsonv_bs_push(T *s, bool value) {
  /*#region*/
  if (jsonv_bs_is_full(s))
    return -1;

  s->top++;
  int byteIdx = s->top / 8;
  int bitIdx = s->top % 8;

  if (value) {
    // Set bit to 1 using OR operator
    s->bytes[byteIdx] |= (1 << bitIdx);
  } else {
    // Set bit to 0 using AND operator with inverted mask
    s->bytes[byteIdx] &= ~(1 << bitIdx);
  }
  return value;
  /*#endregion*/
}

int jsonv_bs_pop(T *s) {
  /*#region*/
  if (jsonv_bs_is_empty(s))
    return -1;

  int byteIdx = s->top / 8;
  int bitIdx = s->top % 8;

  // Extract the bit value
  bool value = (s->bytes[byteIdx] >> bitIdx) & 1;

  s->top--;
  return value;
  /*#endregion*/
}

size_t jsonv_bs_sizeof() {
  /*#region*/
  return sizeof(T);
  /*#endregion*/
}
