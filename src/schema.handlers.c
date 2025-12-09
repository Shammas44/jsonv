#include "schema.handlers.h"
#include "assert.h"
#include "compile.h"
#include "hint.h"
#include "mem.h"
#include "number.h"
#include "schema.h"
#include "validate.handlers.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SIMPLE_HANDLER(validator)                                              \
  {                                                                            \
    jsmntok_t *value = ctx->value;                                             \
    jsmntok_t *key = ctx->key;                                                 \
    Jsonv_SchemaNode *node = ctx->node;                                        \
    List list = node->value_contraints;                                        \
    Jsonv_Contraint *c = CALLOC(1, sizeof(Jsonv_Contraint));                   \
    c->name = (jsonv_Hint){.start = key->start, .end = key->end};              \
    c->value = (jsonv_Hint){.start = value->start, .end = value->end};         \
    c->fn = validator;                                                         \
    if (list) {                                                                \
      list_push(list, c);                                                      \
    } else {                                                                   \
      node->value_contraints = list_new(c, NULL);                              \
    }                                                                          \
    return 0;                                                                  \
  }

#define ERROR(format, ...)                                                     \
  char *p = jsonv_build_data_path(ctx->data, ctx->json_data);                  \
  jsonv_path_reset(ctx->path);                                                 \
  JSONV_ENTER_FIELD(ctx->path, p);                                             \
  JSONV_ERR(ctx->errors, ctx->path, format, __VA_ARGS__);                      \
  JSONV_LEAVE(ctx->path);

// ========================================================
// CONTRAINT: Common properties
// ========================================================

static int handler_type(jsonv_Schema_Context *ctx) {
  /*#region*/
  const char *json = ctx->json;
  jsmntok_t *value_token = ctx->value;
  Jsonv_SchemaNode *node = ctx->schema;
  // TODO use stack memory here
  char *type_str = jsonv_extract_token_string(json, value_token);
  if (!type_str) {
    jsonv_schema_free(ctx->schema);
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

static int handler_required(jsonv_Schema_Context *ctx) {
  /*#region*/
  const char *json = ctx->json;
  jsmntok_t *value_token = ctx->value;
  Jsonv_SchemaNode *schema = ctx->schema;
  jsonv_tokiterator *it = ctx->it;
  assert(value_token->type == JSMN_ARRAY);

  size_t required_count = value_token->size;
  char **required_keys = CALLOC(required_count, sizeof(char *));
  schema->required_keys = required_keys;
  schema->required_keys_length = required_count;

  for (size_t j = 0; j < required_count; j++) {
    jsmntok_t *item_token = jsonv_tokiterator_next(it);
    required_keys[j] = jsonv_extract_token_string(json, item_token);
    if (!required_keys[j]) {
      for (size_t k = 0; k < j; k++) {
        free(required_keys[k]);
      }
      free(required_keys);
      jsonv_schema_free(schema);
      return 1;
    }
  }
  return 0;
  /*#endregion*/
}

// ========================================================
// CONTRAINT: Object Keywords (for `object`)
// ========================================================

static int handler_properties(jsonv_Schema_Context *ctx) {
  /*#region*/
  const char *json = ctx->json;
  jsmntok_t *value_token = ctx->value;
  Jsonv_SchemaNode *node = ctx->schema;
  jsonv_tokiterator *it = ctx->it;
  assert(ctx->value->type == JSMN_OBJECT);

  node->property_count = value_token->size;
  node->properties = CALLOC(node->property_count, sizeof(Jsonv_SchemaNode));

  for (size_t j = 0; j < node->property_count; j++) {
    jsmntok_t *key_token = jsonv_tokiterator_relative(it, 1);
    jsonv_tokiterator_next(it); // eat 'bracket'
    jsonv_tokiterator_next(it); // eat 'key'
    node->properties[j].key =
        (jsonv_Hint){.start = key_token->start, .end = key_token->end};
    node->properties[j] = *jsonv_compile_schema(json, it, node, ctx->path, ctx->errors);
    //TODO key must be set in jsonv_compile_schema
  }
  return 0;
  /*#endregion*/
}

static int validate_maxProperties(void *x) {
  /*#region*/
  ValidatorCtx *ctx = x;
  Jsonv_Contraint *c = ctx->contraint;

  char key_buff[100] = {0};
  char value_buff[100] = {0};
  char given_buff[100] = {0};

  HINT(ctx->json_schema, c->name, key_buff);
  HINT(ctx->json_schema, c->value, value_buff);
  HINT(ctx->json_data, ctx->data->value, given_buff);

  size_t expected = atoi(value_buff);
  size_t given = ctx->data->property_count;

  if (given < expected) {
    return 1;
  } else {
    ERROR("[maxProperties] Expected properties count to be below %d.",
          expected);
    return 0;
  }
  /*#endregion*/
}

static int handler_maxProperties(jsonv_Schema_Context *ctx) {
  /*#region*/
  jsmntok_t *value = ctx->value;
  jsmntok_t *key = ctx->key;
  Jsonv_SchemaNode *node = ctx->schema;
  List list = node->value_contraints;
  Jsonv_Contraint *c = CALLOC(1, sizeof(Jsonv_Contraint));

  char value_buff[100] = {0};
  HINT(ctx->json, c->value, value_buff);
  int given = 0;
  int e = parse_int(value_buff, &given);

  if (!e) {
    char key_buff[100] = {0};
    HINT(ctx->json, node->key, key_buff);
    char *p = jsonv_build_schema_path(node, ctx->json);
    jsonv_path_reset(ctx->path);
    JSONV_ENTER_FIELD(ctx->path, p);
    JSONV_ERR(ctx->errors, ctx->path,
              "[maxProperties] Expected integer value.");
    JSONV_LEAVE(ctx->path);
    return 1;
  }

  c->name = (jsonv_Hint){.start = key->start, .end = key->end};
  c->value = (jsonv_Hint){.start = value->start, .end = value->end};
  c->fn = validate_maxProperties;
  if (list) {
    node->value_contraints = list_push(list, c);
  } else {
    node->value_contraints = list_new(c, NULL);
  }
  return 0;
  /*#endregion*/
}

static int validate_minProperties(void *x) {
  /*#region*/
  ValidatorCtx *ctx = x;
  Jsonv_Contraint *c = ctx->contraint;

  char key_buff[100] = {0};
  char value_buff[100] = {0};
  char given_buff[100] = {0};

  HINT(ctx->json_schema, c->name, key_buff);
  HINT(ctx->json_schema, c->value, value_buff);
  HINT(ctx->json_data, ctx->data->value, given_buff);

  size_t expected = atoi(value_buff);
  size_t given = ctx->data->property_count;

  if (given > expected) {
    return 1;
  } else {
    ERROR("[minProperties] Expected properties count to be at least %d.",
          expected);
    return 0;
  }
  /*#endregion*/
}

static int handler_minProperties(jsonv_Schema_Context *ctx) {
  /*#region*/
  jsmntok_t *value = ctx->value;
  jsmntok_t *key = ctx->key;
  Jsonv_SchemaNode *node = ctx->schema;
  List list = node->value_contraints;
  Jsonv_Contraint *c = CALLOC(1, sizeof(Jsonv_Contraint));
  c->name = (jsonv_Hint){.start = key->start, .end = key->end};
  c->value = (jsonv_Hint){.start = value->start, .end = value->end};
  c->fn = validate_minProperties;
  if (list) {
    node->value_contraints = list_push(list, c);
  } else {
    node->value_contraints = list_new(c, NULL);
  }
  return 0;
  /*#endregion*/
}

// ========================================================
// CONTRAINT: Array Keywords (for `array`)
// ========================================================

static int handler_items(jsonv_Schema_Context *ctx) {
  /*#region*/
  const char *json = ctx->json;
  Jsonv_SchemaNode *node = ctx->schema;
  jsonv_tokiterator *it = ctx->it;
  assert(ctx->value->type == JSMN_OBJECT);
  node->items = CALLOC(1, sizeof(Jsonv_SchemaNode));
  ctx->schema = node->items;
  node->items = jsonv_compile_schema(json, it, node->parent, ctx->path, ctx->errors);
  return 0;
  /*#endregion*/
}

// ========================================================
// CONTRAINT: Numeric Keywords (for `number` and `integer`)
// ========================================================

static int validate_multiplOf(void *x) {
  /*#region*/
  ValidatorCtx *ctx = x;
  Jsonv_Contraint *c = ctx->contraint;

  char key_buff[100] = {0};
  char value_buff[100] = {0};
  char given_buff[100] = {0};

  HINT(ctx->json_schema, c->name, key_buff);
  HINT(ctx->json_schema, c->value, value_buff);
  HINT(ctx->json_data, ctx->data->value, given_buff);

  int expected = atoi(value_buff);
  int given = atoi(given_buff);

  if (given % expected == 0 && given > 0) {
    return 1;
  } else {
    ERROR("[multipleOf] Expected value to be non zero and multiple of %d.",
          expected);
    return 0;
  }
  /*#endregion*/
}

static int handler_multipleOf(jsonv_Schema_Context *ctx) {
  /*#region*/
  jsmntok_t *value = ctx->value;
  jsmntok_t *key = ctx->key;
  Jsonv_SchemaNode *node = ctx->schema;
  List list = node->value_contraints;
  Jsonv_Contraint *c = CALLOC(1, sizeof(Jsonv_Contraint));
  c->name = (jsonv_Hint){.start = key->start, .end = key->end};
  c->value = (jsonv_Hint){.start = value->start, .end = value->end};
  c->fn = validate_multiplOf;
  if (list) {
    node->value_contraints = list_push(list, c);
  } else {
    node->value_contraints = list_new(c, NULL);
  }
  return 0;
  /*#endregion*/
}

static int validate_maximum(void *x) {
  /*#region*/
  ValidatorCtx *ctx = x;
  Jsonv_Contraint *c = ctx->contraint;

  char key_buff[100] = {0};
  char value_buff[100] = {0};
  char given_buff[100] = {0};

  HINT(ctx->json_schema, c->name, key_buff);
  HINT(ctx->json_schema, c->value, value_buff);
  HINT(ctx->json_data, ctx->data->value, given_buff);

  double expected = atof(value_buff);
  double given = atof(given_buff);

  if (given <= expected) {
    return 1;
  } else {
    ERROR("[maximum] Expected value to be below %f.", expected);
    return 0;
  }
  /*#endregion*/
}

static int handler_maximum(jsonv_Schema_Context *ctx) {
  /*#region*/
  jsmntok_t *value = ctx->value;
  jsmntok_t *key = ctx->key;
  Jsonv_SchemaNode *node = ctx->schema;
  List list = node->value_contraints;
  Jsonv_Contraint *c = CALLOC(1, sizeof(Jsonv_Contraint));
  c->name = (jsonv_Hint){.start = key->start, .end = key->end};
  c->value = (jsonv_Hint){.start = value->start, .end = value->end};
  c->fn = validate_maximum;
  if (list) {
    node->value_contraints = list_push(list, c);
  } else {
    node->value_contraints = list_new(c, NULL);
  }
  return 0;
  /*#endregion*/
}

static int validate_exclusiveMaximum(void *x) {
  /*#region*/
  ValidatorCtx *ctx = x;
  Jsonv_Contraint *c = ctx->contraint;

  char key_buff[100] = {0};
  char value_buff[100] = {0};
  char given_buff[100] = {0};

  HINT(ctx->json_schema, c->name, key_buff);
  HINT(ctx->json_schema, c->value, value_buff);
  HINT(ctx->json_data, ctx->data->value, given_buff);

  double expected = atof(value_buff);
  double given = atof(given_buff);

  if (given > expected) {
    return 1;
  } else {
    ERROR("[exclusiveMaximum] Expected value to be strictly below %f.",
          expected);
    return 0;
  }
  /*#endregion*/
}

static int handler_exclusiveMaximum(jsonv_Schema_Context *ctx) {
  /*#region*/
  jsmntok_t *value = ctx->value;
  jsmntok_t *key = ctx->key;
  Jsonv_SchemaNode *node = ctx->schema;
  List list = node->value_contraints;
  Jsonv_Contraint *c = CALLOC(1, sizeof(Jsonv_Contraint));
  c->name = (jsonv_Hint){.start = key->start, .end = key->end};
  c->value = (jsonv_Hint){.start = value->start, .end = value->end};
  c->fn = validate_exclusiveMaximum;
  if (list) {
    node->value_contraints = list_push(list, c);
  } else {
    node->value_contraints = list_new(c, NULL);
  }
  return 0;
  /*#endregion*/
}

static int validate_exclusiveMinimum(void *x) {
  /*#region*/
  ValidatorCtx *ctx = x;
  Jsonv_Contraint *c = ctx->contraint;

  char key_buff[100] = {0};
  char value_buff[100] = {0};
  char given_buff[100] = {0};

  HINT(ctx->json_schema, c->name, key_buff);
  HINT(ctx->json_schema, c->value, value_buff);
  HINT(ctx->json_data, ctx->data->value, given_buff);

  double expected = atof(value_buff);
  double given = atof(given_buff);

  if (given > expected) {
    return 1;
  } else {
    ERROR("[exclusiveMinimum] Expected value to be strictly above %f.",
          expected);
    return 0;
  }
  /*#endregion*/
}

static int handler_exclusiveMinimum(jsonv_Schema_Context *ctx) {
  /*#region*/
  jsmntok_t *value = ctx->value;
  jsmntok_t *key = ctx->key;
  Jsonv_SchemaNode *node = ctx->schema;
  List list = node->value_contraints;
  Jsonv_Contraint *c = CALLOC(1, sizeof(Jsonv_Contraint));
  c->name = (jsonv_Hint){.start = key->start, .end = key->end};
  c->value = (jsonv_Hint){.start = value->start, .end = value->end};
  c->fn = validate_exclusiveMinimum;
  if (list) {
    node->value_contraints = list_push(list, c);
  } else {
    node->value_contraints = list_new(c, NULL);
  }
  return 0;
  /*#endregion*/
}

static int validate_minimum(void *x) {
  /*#region*/
  ValidatorCtx *ctx = x;
  Jsonv_Contraint *c = ctx->contraint;

  char key_buff[100] = {0};
  char value_buff[100] = {0};
  char given_buff[100] = {0};

  HINT(ctx->json_schema, c->name, key_buff);
  HINT(ctx->json_schema, c->value, value_buff);
  HINT(ctx->json_data, ctx->data->value, given_buff);

  double expected = atof(value_buff);
  double given = atof(given_buff);

  if (given >= expected) {
    return 1;
  } else {
    ERROR("[minimum] Expected value to be above %f.", expected);
    return 0;
  }
  /*#endregion*/
}

static int handler_minimum(jsonv_Schema_Context *ctx) {
  /*#region*/
  jsmntok_t *value = ctx->value;
  jsmntok_t *key = ctx->key;
  Jsonv_SchemaNode *node = ctx->schema;
  List list = node->value_contraints;
  Jsonv_Contraint *c = CALLOC(1, sizeof(Jsonv_Contraint));
  c->name = (jsonv_Hint){.start = key->start, .end = key->end};
  c->value = (jsonv_Hint){.start = value->start, .end = value->end};
  c->fn = validate_minimum;
  if (list) {
    node->value_contraints = list_push(list, c);
  } else {
    node->value_contraints = list_new(c, NULL);
  }
  return 0;
  /*#endregion*/
}

// ========================================================
// CONTRAINT: String Keywords (for `string`)
// ========================================================

static int validate_minLength(void *x) {
  /*#region*/
  ValidatorCtx *ctx = x;
  Jsonv_Contraint *c = ctx->contraint;

  char key_buff[100] = {0};
  char value_buff[100] = {0};
  char given_buff[100] = {0};

  HINT(ctx->json_schema, c->name, key_buff);
  HINT(ctx->json_schema, c->value, value_buff);
  HINT(ctx->json_data, ctx->data->value, given_buff);
  int given = ctx->data->value.end - ctx->data->value.start;

  int expected = atoi(value_buff);

  if (given >= expected && given >= 0) {
    return 1;
  } else {
    ERROR("[minLength] Expected length to be non-negative and above or "
          "equal to %d.",
          expected);
    return 0;
  }
  /*#endregion*/
}

static int handler_minLength(jsonv_Schema_Context *ctx) {
  /*#region*/
  jsmntok_t *value = ctx->value;
  jsmntok_t *key = ctx->key;
  Jsonv_SchemaNode *node = ctx->schema;
  List list = node->value_contraints;
  Jsonv_Contraint *c = CALLOC(1, sizeof(Jsonv_Contraint));
  c->name = (jsonv_Hint){.start = key->start, .end = key->end};
  c->value = (jsonv_Hint){.start = value->start, .end = value->end};
  c->fn = validate_minLength;
  if (list) {
    node->value_contraints = list_push(list, c);
  } else {
    node->value_contraints = list_new(c, NULL);
  }
  return 0;
  /*#endregion*/
}

static int validate_maxLength(void *x) {
  /*#region*/
  ValidatorCtx *ctx = x;
  Jsonv_Contraint *c = ctx->contraint;

  char key_buff[100] = {0};
  char value_buff[100] = {0};
  char given_buff[100] = {0};

  HINT(ctx->json_schema, c->name, key_buff);
  HINT(ctx->json_schema, c->value, value_buff);
  HINT(ctx->json_data, ctx->data->value, given_buff);
  int given = ctx->data->value.end - ctx->data->value.start;

  int expected = atoi(value_buff);

  if (given <= expected && given >= 0) {
    return 1;
  } else {
    ERROR("[maxLength] Expected length to be non-negative and less or "
          "equal to %d.",
          expected);
    return 0;
  }
  /*#endregion*/
}

static int handler_maxLength(jsonv_Schema_Context *ctx) {
  /*#region*/
  jsmntok_t *value = ctx->value;
  jsmntok_t *key = ctx->key;
  Jsonv_SchemaNode *node = ctx->schema;
  List list = node->value_contraints;
  Jsonv_Contraint *c = CALLOC(1, sizeof(Jsonv_Contraint));
  c->name = (jsonv_Hint){.start = key->start, .end = key->end};
  c->value = (jsonv_Hint){.start = value->start, .end = value->end};
  c->fn = validate_maxLength;
  if (list) {
    node->value_contraints = list_push(list, c);
  } else {
    node->value_contraints = list_new(c, NULL);
  }
  return 0;
  /*#endregion*/
}

#define HANDLER(name)                                                          \
  { #name, handler_##name }

Schema_Handler g_handlers[] = {
    // common
    HANDLER(type),
    HANDLER(required),
    // object
    HANDLER(properties),
    HANDLER(minProperties),
    HANDLER(maxProperties),
    // array
    HANDLER(items),
    // number
    HANDLER(multipleOf),
    HANDLER(maximum),
    HANDLER(minimum),
    HANDLER(exclusiveMaximum),
    HANDLER(exclusiveMinimum),
    // number
    HANDLER(maxLength),
    HANDLER(minLength),
};

size_t g_handlers_length = sizeof(g_handlers) / sizeof(g_handlers[0]);
