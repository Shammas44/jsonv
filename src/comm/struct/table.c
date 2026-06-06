#include "table.h"
#include "assert.h"
#include "mem.h"
#include <limits.h>
#include <stddef.h>
#define T Table

struct T {
  int size;
  int (*cmp)(const void *x, const void *y);
  unsigned (*hash)(const void *key);
  int length;
  unsigned timestamp;
  struct binding {
    struct binding *link;
    const void *key;
    void *value;
  } **buckets;
};

static int cmpatom(const void *x, const void *y) {
  /*#region*/
  return x != y;
  /*#endregion*/
}

static unsigned hashatom(const void *key) {
  /*#region*/
  return (unsigned long)key >> 2;
  /*#endregion*/
}

T *table_new(int hint, int cmp(const void *x, const void *y),
             unsigned hash(const void *key)) {
  /*#region*/
  T *table;
  int i;
  static int primes[] = {509,  509,   1021,  2053,  4093,
                         8191, 16381, 32771, 65521, INT_MAX};
  assert(hint >= 0);
  for (i = 1; primes[i] < hint; i++)
    ;
  table = ALLOC(sizeof(*table) + primes[i - 1] * sizeof(table->buckets[0]));
  table->size = primes[i - 1];
  table->cmp = cmp ? cmp : cmpatom;
  table->hash = hash ? hash : hashatom;
  table->buckets = (struct binding **)(table + 1);
  for (i = 0; i < table->size; i++)
    table->buckets[i] = NULL;
  table->length = 0;
  table->timestamp = 0;
  return table;
  /*#endregion*/
}

void *table_get(T *table, const void *key) {
  /*#region*/
  int i;
  struct binding *p;
  assert(table);
  assert(key);
  i = (*table->hash)(key) % table->size;
  for (p = table->buckets[i]; p; p = p->link)
    if ((*table->cmp)(key, p->key) == 0)
      break;
  return p ? p->value : NULL;
  /*#endregion*/
}

void *table_put(T *table, const void *key, void *value) {
  /*#region*/
  int i;
  struct binding *p;
  void *prev;
  assert(table);
  assert(key);
  i = (*table->hash)(key) % table->size;
  for (p = table->buckets[i]; p; p = p->link)
    if ((*table->cmp)(key, p->key) == 0)
      break;
  if (p == NULL) {
    NEW(p);
    p->key = key;
    p->link = table->buckets[i];
    table->buckets[i] = p;
    table->length++;
    prev = NULL;
  } else
    prev = p->value;
  p->value = value;
  table->timestamp++;
  return prev;
  /*#endregion*/
}

int table_length(T *table) {
  /*#region*/
  assert(table);
  return table->length;
  /*#endregion*/
}

void table_map(T *table, void apply(const void *key, void **value, void *cl),
               void *cl) {
  /*#region*/
  int i;
  unsigned stamp;
  struct binding *p;
  assert(table);
  assert(apply);
  stamp = table->timestamp;
  for (i = 0; i < table->size; i++)
    for (p = table->buckets[i]; p; p = p->link) {
      apply(p->key, &p->value, cl);
      assert(table->timestamp == stamp);
    }
  /*#endregion*/
}

void *table_remove(T *table, const void *key) {
  /*#region*/
  int i;
  struct binding **pp;
  assert(table);
  assert(key);
  table->timestamp++;
  i = (*table->hash)(key) % table->size;
  for (pp = &table->buckets[i]; *pp; pp = &(*pp)->link)
    if ((*table->cmp)(key, (*pp)->key) == 0) {
      struct binding *p = *pp;
      void *value = p->value;
      *pp = p->link;
      FREE(p);
      table->length--;
      return value;
    }
  return NULL;
  /*#endregion*/
}

TableEntry *table_to_array(T *table, int *count) {
  /*#region*/
  assert(table);

  int total = table->length;
  if (count)
    *count = total;

  // Allocate array of entries (+1 if you want a sentinel)
  TableEntry *array = ALLOC((total + 1) * sizeof(TableEntry));

  int j = 0;
  for (int i = 0; i < table->size; i++) {
    struct binding *p;
    for (p = table->buckets[i]; p; p = p->link) {
      array[j].key = (void *)p->key;
      array[j].value = p->value;
      j++;
    }
  }

  // Optional: sentinel entry
  array[j].key = NULL;
  array[j].value = NULL;

  return array;
  /*#endregion*/
}

void table_free(T **table, void apply(void *key, void *value)) {
  /*#region*/
  assert(table && *table);
  if ((*table)->length > 0) {
    int i;
    struct binding *p, *q;
    for (i = 0; i < (*table)->size; i++)
      for (p = (*table)->buckets[i]; p; p = q) {
        q = p->link;
        if (apply)
          apply((void *)p->key, p->value);
        FREE(p);
      }
  }
  FREE(*table);
  /*#endregion*/
}

T *table_fuse(T *a, T *b) {
  /*#region*/
  assert(a);
  assert(b);
  int len1 = table_length(a);
  int len2 = table_length(b);
  T *output = table_new(len1 + len2, NULL, NULL);

  unsigned stamp;
  struct binding *p;
  stamp = a->timestamp;
  for (int i = 0; i < a->size; i++)
    for (p = a->buckets[i]; p; p = p->link) {
      table_put(output, p->key, p->value);
      assert(a->timestamp == stamp);
    }
  stamp = b->timestamp;
  for (int i = 0; i < b->size; i++)
    for (p = b->buckets[i]; p; p = p->link) {
      table_put(output, p->key, p->value);
      assert(b->timestamp == stamp);
    }
  return output;
  /*#endregion*/
}
