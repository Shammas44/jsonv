#ifndef _jsonv_UTILS_H_INCLUDED
#define _jsonv_UTILS_H_INCLUDED
#include "error.h"
#include "stack.h"

#define JSONV_SCHEMA_IS_VALID -1

typedef struct {
  char description[100];
  char *path;
  Jsonv_Error_Type type;
} E;

int validate_against_schema(Stack *data, int data_root, Stack *schema,
                            int schema_root, E **error);
#endif
