#include "mem.h"
#include "assert.h"
#include "except.h"
#include <stddef.h>
#include <stdlib.h>

const Except Mem_Failed = {"Allocation failed"};

void *mem_alloc(long nbytes, const char *file, int line) {
  /*#region*/
  void *ptr;
  assert(nbytes > 0);
  ptr = malloc(nbytes);
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
  ptr = calloc(count, nbytes);
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
