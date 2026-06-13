#include "mem.h"
#include "assert.h"
#include "except.h"
#include "arena.h"
#include "arena.internal.h"
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

const Except Mem_Failed = {"Allocation failed", Jsonv_Mem_Failed};

void *(*g_jsonv_malloc)(size_t) = malloc;
void *(*g_jsonv_calloc)(size_t, size_t) = calloc;

static Jsonv_Arena *jsonv_internal_arena = NULL;
static bool initializing_arena = false;

Jsonv_Arena *jsonv_get_internal_arena(void) {
  /*#region*/
  if (!jsonv_internal_arena && !initializing_arena) {
    initializing_arena = true;
    jsonv_internal_arena = jsonv_arena_new(65536, 128 * 1024 * 1024, 1024 * 1024);
    initializing_arena = false;
  }
  return jsonv_internal_arena;
  /*#endregion*/
}

void *mem_alloc(long nbytes, const char *file, int line) {
  /*#region*/
  void *ptr;
  assert(nbytes > 0);
  ptr = g_jsonv_malloc(nbytes);
  if (ptr == NULL) {
    if (file == NULL)
      RAISE(Mem_Failed);
    else
      Except_raise(&Mem_Failed, file, line);
  }
  return ptr;
  /*#endregion*/
}

void *mem_calloc(long count, long nbytes, const char *file, int line) {
  /*#region*/
  void *ptr;
  assert(count > 0);
  assert(nbytes > 0);
  ptr = g_jsonv_calloc(count, nbytes);
  if (ptr == NULL) {
    if (file == NULL)
      RAISE(Mem_Failed);
    else
      Except_raise(&Mem_Failed, file, line);
  }
  return ptr;
  /*#endregion*/
}

void mem_free(void *ptr, const char *file, int line) {
  /*#region*/
  (void)(file);
  (void)(line);
  if (ptr)
    free(ptr);
  /*#endregion*/
}

void *mem_resize(void *ptr, long nbytes, const char *file, int line) {
  /*#region*/
  assert(ptr);
  assert(nbytes > 0);
  ptr = realloc(ptr, nbytes);
  if (ptr == NULL) {
    if (file == NULL)
      RAISE(Mem_Failed);
    else
      Except_raise(&Mem_Failed, file, line);
  }
  return ptr;
  /*#endregion*/
}

void *mem_alloc_internal(long nbytes, const char *file, int line) {
  /*#region*/
  Jsonv_Arena *arena = jsonv_get_internal_arena();
  if (!arena) {
    return mem_alloc(nbytes, file, line);
  }
  void *ptr = jsonv_arena_alloc(arena, nbytes);
  if (ptr == NULL) {
    if (file == NULL)
      RAISE(Mem_Failed);
    else
      Except_raise(&Mem_Failed, file, line);
  }
  return ptr;
  /*#endregion*/
}

void *mem_calloc_internal(long count, long nbytes, const char *file, int line) {
  /*#region*/
  Jsonv_Arena *arena = jsonv_get_internal_arena();
  if (!arena) {
    return mem_calloc(count, nbytes, file, line);
  }
  void *ptr = jsonv_arena_alloc(arena, count * nbytes);
  if (ptr == NULL) {
    if (file == NULL)
      RAISE(Mem_Failed);
    else
      Except_raise(&Mem_Failed, file, line);
  }
  memset(ptr, 0, count * nbytes);
  return ptr;
  /*#endregion*/
}

void mem_free_internal(void *ptr, const char *file, int line) {
  /*#region*/
  (void)ptr;
  (void)file;
  (void)line;
  /*#endregion*/
}

size_t jsonv_get_internal_arena_used_bytes(void) {
  /*#region*/
  if (jsonv_internal_arena) {
    return arena_get_total_reserved(jsonv_internal_arena);
  }
  return 0;
  /*#endregion*/
}
