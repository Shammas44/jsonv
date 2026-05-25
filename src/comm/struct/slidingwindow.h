#ifndef _JSONV_SLIDINGWINDOW_H_INCLUDED
#define _JSONV_SLIDINGWINDOW_H_INCLUDED
#include "token.h"
#define T Jsonv_SlidingWindow

#define WINDOW_SIZE 3

typedef struct {
  Token data[WINDOW_SIZE];
  int head;  // Where the next element goes
  int count; // How many elements are currently in the window
} T;

void jsonv_sw_new(T *w);

int jsonv_sw_count(T *w);

void jsonv_sw_push(T *w, Token value);

Token jsonv_sw_get_by_order(T *w, int index);

#undef T
#endif
