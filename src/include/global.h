#ifndef GLOBALS_H
#define GLOBALS_H
#include <unistd.h>

extern void *(*g_jsonv_malloc)(size_t);
extern void *(*g_jsonv_calloc)(size_t, size_t);

#endif
