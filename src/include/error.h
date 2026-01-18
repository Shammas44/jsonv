#ifndef _JSONV_ERROR_H_INCLUDED
#define _JSONV_ERROR_H_INCLUDED
#include <stdarg.h>
#include <stdbool.h>
#include <stdlib.h>

#define JSONV_MAX_PATH_LEN 512
#define JSONV_MAX_ERR_LEN 1024

/* ------------------------------
    JSON PATH BUILDER
    ------------------------------ */

typedef struct {
  char **segments;
  size_t count;
  size_t capacity;
} Jsonv_path;

bool jsonv_path_init(Jsonv_path *p);
void jsonv_path_free(Jsonv_path *p);
void jsonv_path_reset(Jsonv_path *p);
bool jsonv_path_push(Jsonv_path *p, const char *segment);
bool jsonv_path_push_index(Jsonv_path *p, int index);
void jsonv_path_pop(Jsonv_path *p);
char *jsonv_path_get(const Jsonv_path *p);

typedef struct {
  char **messages;
  size_t count;
  size_t capacity;
} Jsonv_error_stack;

bool jsonv_error_init(Jsonv_error_stack *s);
void jsonv_error_free(Jsonv_error_stack *s);

void jsonv_error_reset(Jsonv_error_stack *s);

void jsonv_error_push(Jsonv_error_stack *s, const char *fmt, ...);
void jsonv_error_print(const Jsonv_error_stack *s);

#define JSONV_ERR(s, path, fmt, ...)                                           \
  do {                                                                         \
    char *heap_path = jsonv_path_get((path));                                  \
    char *heap_msg = ALLOC(JSONV_MAX_ERR_LEN);                                 \
    snprintf(heap_msg, JSONV_MAX_ERR_LEN, fmt, ##__VA_ARGS__);                 \
    jsonv_error_push((s), "%s %s", heap_path, heap_msg);                       \
    free(heap_path);                                                           \
  } while (0);

#define JSONV_ENTER_FIELD(path, name) jsonv_path_push((path), name)
#define JSONV_ENTER_INDEX(path, idx) jsonv_path_push_index((path), idx)
#define JSONV_LEAVE(path) jsonv_path_pop((path))

typedef enum {
  // number
  Jsonv_Maximum_error,
  Jsonv_Minimum_error,
  Jsonv_MultipleOf_error,
  Jsonv_ExclusiveMaximum_error,
  Jsonv_ExclusiveMinimum_error,
  // string
  Jsonv_MaxLength_error,
  Jsonv_MinLength_error,
  // object
  Jsonv_Type_error,
  Jsonv_AdditionalProperties_error,
  // array
  Jsonv_MinItems_error,
  Jsonv_Required_error,
  // else
  Jsonv_ValueNotAllowed_error,
} Jsonv_Error_Type;

#endif
