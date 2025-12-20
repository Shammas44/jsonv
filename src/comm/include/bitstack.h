#ifndef _JSONV_bitstack_H_INCLUDED
#define _JSONV_bitstack_H_INCLUDED
#include <stdbool.h>
#include <stdio.h>

#define MAX_BITS 128
#define T Jsonv_BitStack

typedef struct T T;

void jsonv_bs_init(T *s);

bool jsonv_bs_is_full(T *s);

int jsonv_bs_top(T *s);

bool jsonv_bs_is_empty(T *s);

int jsonv_bs_push(T *s, bool value);

int jsonv_bs_pop(T *s);

size_t jsonv_bs_sizeof();

#undef T
#endif
