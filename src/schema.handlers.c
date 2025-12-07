#include "schema.handlers.h"
#include "compile.h"
#include "schema.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int jsonv_schema_handler_type(jsonv_Schema_Context *ctx) {
  /*#region*/
  const char *json = ctx->json;
  jsmntok_t *value_token = ctx->value;
  Jsonv_SchemaNode *node = ctx->node;
  char *type_str = jsonv_extract_token_string(json, value_token);
  if (!type_str) {
    jsonv_schema_free(ctx->node);
    return 1;
  }

  if (strcmp(type_str, "string") == 0)
    node->type = jsonv_STRING;
  else if (strcmp(type_str, "number") == 0)
    node->type = jsonv_NUMBER;
  else if (strcmp(type_str, "integer") == 0)
    node->type = jsonv_INTEGER;
  else if (strcmp(type_str, "boolean") == 0)
    node->type = jsonv_BOOLEAN;
  else if (strcmp(type_str, "object") == 0)
    node->type = jsonv_OBJECT;
  else if (strcmp(type_str, "array") == 0)
    node->type = jsonv_ARRAY;

  free(type_str);
  return 0;
  /*#endregion*/
}

int jsonv_schema_handler_required(jsonv_Schema_Context *ctx) {
  /*#region*/
  const char *json = ctx->json;
  jsmntok_t *value_token = ctx->value;
  Jsonv_SchemaNode *node = ctx->node;
  jsonv_tokiterator *it = ctx->it;
  if (value_token->type != JSMN_ARRAY) {
    jsonv_schema_free(node);
    return 1;
  }

  size_t required_count = value_token->size;
  char **required_keys = (char **)calloc(required_count, sizeof(char *));
  node->required_keys = required_keys;
  node->required_keys_length = required_count;
  if (!required_keys) {
    jsonv_schema_free(node);
    return 1;
  }

  for (size_t j = 0; j < required_count; j++) {
    jsmntok_t *item_token = jsonv_tokiterator_next(it);
    required_keys[j] = jsonv_extract_token_string(json, item_token);
    if (!required_keys[j]) {
      for (size_t k = 0; k < j; k++) {
        free(required_keys[k]);
      }
      free(required_keys);
      jsonv_schema_free(node);
      return 1;
    }
  }
  return 0;
  /*#endregion*/
}

int jsonv_schema_handler_properties(jsonv_Schema_Context *ctx) {
  /*#region*/
  const char *json = ctx->json;
  jsmntok_t *value_token = ctx->value;
  Jsonv_SchemaNode *node = ctx->node;
  jsonv_tokiterator *it = ctx->it;

  if (ctx->value->type != JSMN_OBJECT) {
    jsonv_schema_free(node);
    return 1;
  }

  node->property_count = value_token->size;
  node->properties = (Jsonv_SchemaNode *)calloc(node->property_count,
                                                sizeof(Jsonv_SchemaNode));
  if (!node->properties) {
    jsonv_schema_free(node);
    return 1;
  }
  for (size_t j = 0; j < node->property_count; j++) {
    jsmntok_t *key_token = jsonv_tokiterator_relative(it, 1);
    jsonv_tokiterator_next(it); // eat 'bracket'
    jsonv_tokiterator_next(it); // eat 'key'
    node->properties[j] = *jsonv_compile_schema(json, it);
    node->properties[j].key =
        (jsonv_Hint){.start = key_token->start, .end = key_token->end};
  }
  return 0;
  /*#endregion*/
}

int jsonv_schema_handler_items(jsonv_Schema_Context *ctx) {
  /*#region*/
  const char *json = ctx->json;
  Jsonv_SchemaNode *node = ctx->node;
  jsonv_tokiterator *it = ctx->it;

  if (ctx->value->type != JSMN_OBJECT) {
    jsonv_schema_free(node);
    return 1;
  }

  node->items = (Jsonv_SchemaNode *)calloc(1, sizeof(Jsonv_SchemaNode));
  if (!node->items) {
    jsonv_schema_free(node);
    return 1;
  }

  ctx->node = node->items;
  node->items = jsonv_compile_schema(json, it);
  return 0;
  /*#endregion*/
}
