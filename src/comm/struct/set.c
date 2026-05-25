#include "set.h"
#include <stdio.h>
#include <string.h>
#define T set_t

static inline size_t floor_power_of_two(size_t n) {
  /*#region*/
  if (n == 0) return 0;
  n |= n >> 1;
  n |= n >> 2;
  n |= n >> 4;
  n |= n >> 8;
  n |= n >> 16;
  #if UINTPTR_MAX == UINT64_MAX
  n |= n >> 32;
  #endif
  return n - (n >> 1);
  /*#endregion*/
}

static inline uint32_t hash_str(const char *s, uint32_t len) {
  /*#region*/
  uint32_t h = 2166136261u; /* FNV-1a 32-bit */
  const uint8_t *us = (const uint8_t *)s;
  const uint8_t *end = us + len;
  while (us < end) {
    h ^= *us++;
    h *= 16777619u;
  }
  return h;
  /*#endregion*/
}

inline void set_init(T *set, entry_t*entries, size_t capacity) {
  /*#region*/
  size_t p2_capacity = floor_power_of_two(capacity);
  *set = (T){.__entries=entries, .capacity=p2_capacity, .length = 0};
  memset(entries, 0, sizeof(entry_t) * p2_capacity);
  /*#endregion*/
}

inline size_t set_insert(T *set, const char *key, uint16_t len) {
  /*#region*/
  if (len > MAX_KEY_LEN)
    return SET_KEY_TOO_LONG;

  size_t capacity = set->capacity;
  if (capacity == 0)
    return SET_TABLE_IS_FULL;

  uint32_t hash = hash_str(key, len);
  uint32_t idx = hash & (capacity - 1); // Capacity is guaranteed to be a power of 2!

  for (uint32_t probe = 0; probe < capacity; probe++) {
    entry_t *e = &set->__entries[idx];

    if (!e->used) {
      /* empty slot */
      e->used = 1;
      e->len = len;
      set->length += 1;
      memcpy(e->key, key, len);
      return SET_KEY_IS_UNIQ;
    }

    if (e->len == len && memcmp(e->key, key, len) == 0) {
      return SET_KEY_ALREADY_EXIST;
    }

    // Optimization: Avoid division/modulo inside hot probe loop
    idx++;
    if (idx >= capacity) {
      idx = 0;
    }
  }

  return SET_TABLE_IS_FULL;
  /*#endregion*/
}

inline void set_clear(T *set) {
  /*#region*/
  memset(set, 0, sizeof(T));
  /*#endregion*/
}

inline size_t set_next_power_of_two(size_t capacity) {
  /*#region*/
  if (capacity == 0) return 1;
  capacity--;
  capacity |= capacity >> 1;
  capacity |= capacity >> 2;
  capacity |= capacity >> 4;
  capacity |= capacity >> 8;
  capacity |= capacity >> 16;
  #if UINTPTR_MAX == UINT64_MAX
  capacity |= capacity >> 32;
  #endif
  capacity++;
  return capacity;
  /*#endregion*/
}
