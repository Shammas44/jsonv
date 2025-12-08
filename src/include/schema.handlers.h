#ifndef _JSONV_SCHEMA_HANDLERS_H_INCLUDED
#define _JSONV_SCHEMA_HANDLERS_H_INCLUDED
#include "schema.h"
#include "token.h"

typedef struct {
  Jsonv_SchemaNode *node;
  jsonv_tokiterator *it;
  jsmntok_t *key;
  jsmntok_t *value;
  const char *json;
} jsonv_Schema_Context;

typedef int (*jsonv_Schema_Handler)(jsonv_Schema_Context *ctx);

typedef struct Schema_Handler {
  const char *key;
  jsonv_Schema_Handler handler;
} Schema_Handler;

#endif
