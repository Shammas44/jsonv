#ifndef _JSONV_ERROR_H_INCLUDED
#define _JSONV_ERROR_H_INCLUDED
#include <stdarg.h>

#define JSONV_MAX_ERROR_MSG 256
#define JSONV_MAX_ERROR_STACK 32
#define JSONV_MAX_PATH_SEG 64
#define JSONV_MAX_PATH_LEN 512

/* ------------------------------
   JSON PATH BUILDER
   ------------------------------ */

typedef struct {
  char segments[JSONV_MAX_PATH_SEG][JSONV_MAX_PATH_LEN];
  int count;
} Jsonv_path;

void jsonv_path_reset(Jsonv_path *p);
void jsonv_path_push(Jsonv_path *p, const char *segment);
void jsonv_path_push_index(Jsonv_path *p, int index);
void jsonv_path_pop(Jsonv_path *p);
void jsonv_path_get(const Jsonv_path *p, char out[JSONV_MAX_PATH_LEN]);

/* ------------------------------
   ERROR STACK
   ------------------------------ */

typedef struct {
  char messages[JSONV_MAX_ERROR_STACK][JSONV_MAX_ERROR_MSG];
  int count;
} Jsonv_error_stack;

void jsonv_error_reset(Jsonv_error_stack *s);
void jsonv_error_push(Jsonv_error_stack *s, const char *fmt, ...);
void jsonv_error_print(const Jsonv_error_stack *s);

/* ------------------------------
   MACROS FOR EASY REPORTING
   ------------------------------ */

#define JSONV_ERR(s, path, fmt, ...)                                           \
  jsonv_error_push((s), "%s: " fmt, ({                                         \
                     char __tmp[JSONV_MAX_PATH_LEN];                           \
                     jsonv_path_get((path), __tmp);                            \
                     __tmp;                                                    \
                   }),                                                         \
                   ##__VA_ARGS__)

#define JSONV_ENTER_FIELD(path, name) jsonv_path_push((path), name)
#define JSONV_ENTER_INDEX(path, idx) jsonv_path_push_index((path), idx)
#define JSONV_LEAVE(path) jsonv_path_pop((path))

#endif
