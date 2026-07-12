#ifndef _JSONV_GLOBALS_H
#define _JSONV_GLOBALS_H
#include <unistd.h>
#include "macro.h"

extern void *(*g_jsonv_malloc)(size_t);
extern void *(*g_jsonv_calloc)(size_t, size_t);
extern void (*g_jsonv_free)(void*);

JSONV_API void jsonv_free_all(void);

#endif
