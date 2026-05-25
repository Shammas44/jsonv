#include "except.h"
#include "assert.h"
#include <stdio.h>
#include <stdlib.h>
#define T Except

const Except MALFORMED_JSON = {"Malformed JSON data", Jsonv_Malformed_json};
const Except MAXIMUM_NESTED_DEPTH_REACHED = {
    "Maximum nested depth reached.", Jsonv_Maximum_Nested_Depth_Reached};
const Except ARENA_LIMIT_REACHED = {"Arena limit reached.",
                                    Jsonv_Arena_Limit_Reached};
const Except MAXIMUM_TOKEN_BYTES_REACHED = {"Maximum token's bytes reached.",
                                            Jsonv_Maximum_Token_Bytes_Reached};
const Except MAXIMUM_ARRAY_REACHED = {"Maximum arrays reached.",
                                            Jsonv_Maximum_Array_Reached};
const Except MAXIMUM_OBJECT_REACHED = {"Maximum objects reached.",
                                            Jsonv_Maximum_Object_Reached};
const Except MAXIMUM_VALUES_REACHED = {"Maximum values reached.",
                                            Jsonv_Maximum_Values_Reached};

Except_Frame *Except_stack = NULL;
void Except_raise(const T *e, const char *file, int line) {
#ifdef WIN32
  Except_Frame *p;

  if (Except_index == -1)
    Except_init();
  p = TlsGetValue(Except_index);
#else
  Except_Frame *p = Except_stack;
#endif
  assert(e);
  if (p == NULL) {
    fprintf(stderr, "Uncaught exception");
    if (e->reason)
      fprintf(stderr, " %s", e->reason);
    else
      fprintf(stderr, " at 0x%p", e);
    if (file && line > 0)
      fprintf(stderr, " raised at %s:%d\n", file, line);
    fprintf(stderr, "aborting...\n");
    fflush(stderr);
    abort();
  }
  p->exception = e;
  p->file = file;
  p->line = line;
#ifdef WIN32
  Except_pop();
#else
  Except_stack = Except_stack->prev;
#endif
  longjmp(p->env, Except_raised);
}
#ifdef WIN32
_CRTIMP void __cdecl _assert(void *, void *, unsigned);
#undef assert
#define assert(e) ((e) || (_assert(#e, __FILE__, __LINE__), 0))

int Except_index = -1;
void Except_init(void) {
  BOOL cond;

  Except_index = TlsAlloc();
  assert(Except_index != TLS_OUT_OF_INDEXES);
  cond = TlsSetValue(Except_index, NULL);
  assert(cond == TRUE);
}

void Except_push(Except_Frame *fp) {
  BOOL cond;

  fp->prev = TlsGetValue(Except_index);
  cond = TlsSetValue(Except_index, fp);
  assert(cond == TRUE);
}

void Except_pop(void) {
  BOOL cond;
  Except_Frame *tos = TlsGetValue(Except_index);

  cond = TlsSetValue(Except_index, tos->prev);
  assert(cond == TRUE);
}
#endif
