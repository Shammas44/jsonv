#include "set.h"
#include <stdio.h>
#include <string.h>
#define T set_t

static inline uint32_t hash_str(const char *s, uint32_t len) {
  /*#region*/
  uint32_t h = 2166136261u; /* FNV-1a 32-bit */
  for (uint32_t i = 0; i < len; i++) {
    h ^= (uint8_t)s[i];
    h *= 16777619u;
  }
  return h;
  /*#endregion*/
}

inline void set_init(T *set, entry_t*entries, size_t capacity) {
  /*#region*/
  *set = (T){.__entries=entries, .capacity=capacity, .length = 0};
  memset(entries,0, sizeof(entry_t)* capacity);
  /*#endregion*/
}

inline size_t set_insert(T *set, const char *key, uint16_t len) {
  /*#region*/
  if (len > MAX_KEY_LEN)
    return SET_KEY_TOO_LONG;

  size_t capacity = set->capacity;
  uint32_t idx = hash_str(key, len) % capacity;

  for (uint32_t probe = 0; probe < capacity; probe++) {
    if (!set->__entries[idx].used) {
      /* empty slot */
      set->__entries[idx].used = 1;
      set->__entries[idx].len = len;
      set->length+=1;
      memcpy(set->__entries[idx].key, key, len);
      return SET_KEY_IS_UNIQ;
    }

    entry_t *e = &set->__entries[idx];

    if (e->len == len && memcmp(e->key, key, len) == 0) {
      return SET_KEY_ALREADY_EXIST;
    }

    idx = (idx + 1) % capacity;
  }

  return SET_TABLE_IS_FULL;
  /*#endregion*/
}

inline void set_clear(T *set) {
  /*#region*/
  memset(set, 0, sizeof(T));
  /*#endregion*/
}
