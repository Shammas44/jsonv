#ifndef _JSONV_SET_H
#define _JSONV_SET_H
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#define T set_t

#define MAX_KEY_LEN 64   /* max JSON key length */

#define SET_KEY_IS_UNIQ 0
#define SET_TABLE_IS_FULL 1
#define SET_KEY_ALREADY_EXIST 2
#define SET_KEY_TOO_LONG 3

typedef struct {
  bool used;
  uint16_t len;
  char key[MAX_KEY_LEN];
} entry_t;

typedef struct T {
  size_t capacity;
  size_t length;
  entry_t *__entries;
} T;

void set_init(T *set, entry_t*entries, size_t capacity) ;
size_t set_insert(T *set, const char *key, uint16_t len);
void set_clear(T*set);
size_t set_next_power_of_two(size_t capacity);

#undef T
#endif
