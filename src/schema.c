#include "schema.h"
#include "assert.h"
#include "compile.h"
#include "hint.h"
#include "mem.h"
#include "schema.handlers.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern Schema_Handler g_handlers[];
extern size_t g_handlers_length;

Jsonv_SchemaNode *jsonv_compile_schema(const char *json,
                                       jsonv_tokiterator *it) {
  /*#region*/
  int token_index = jsonv_tokiterator_index(it);
  jsmntok_t *current_token = jsonv_tokiterator_current(it);

  if (token_index < 0 || current_token->type != JSMN_OBJECT) {
    return NULL;
  }

  Jsonv_SchemaNode *node = CALLOC(1, sizeof(Jsonv_SchemaNode));

  // Initialize defaults
  node->type = jsonv_UNKNOWN;
  node->additional_properties = false;

  // --- Pass 1: Parse All Keywords and Recursively Compile Sub-Schemas ---
  for (int i = 0; i < current_token->size; i++) {
    // 1. IDENTIFY KEY and VALUE TOKENS
    jsmntok_t *key_token = jsonv_tokiterator_next(it);
    jsmntok_t *value_token = jsonv_tokiterator_next(it);

    // 2. ISOLATE THE JUMP: Calculate the index of the next KEY token.
    char key[100] = {0};
    TOK(json, *key_token, key);

    jsonv_Schema_Context ctx = {.node = node,
                                .value = value_token,
                                .key = key_token,
                                .json = json,
                                .it = it};

    for (unsigned long j = 0; j < g_handlers_length; j++) {
      if (strcmp(key, g_handlers[j].key) == 0) {
        int e = g_handlers[j].handler(&ctx);
        assert(!e);
        break;
      }
    }
  }

  // --- Pass 2: Merge Required Flags into Properties (Unchanged but validated)
  char **required_keys = node->required_keys;
  size_t required_count = node->required_keys_length;
  if (node->properties && required_keys) {
    for (size_t k = 0; k < required_count; k++) {
      for (size_t p = 0; p < node->property_count; p++) {
        // TODO improve this
        if (required_keys[k] != NULL && node->properties[p].key.start != 0) {
          int len = node->properties[p].key.end - node->properties[p].key.start;
          if (strncmp(required_keys[k], json + node->properties[p].key.start,
                      len) == 0) {
            node->properties[p].required = true;
            break;
          }
        }
      }
    }
  }

  // Clean up temporary required keys array
  if (required_keys) {
    free(required_keys);
  }
  return node;
  /*#endregion*/
}

void jsonv_schema_free(Jsonv_SchemaNode *node) {
  /*#region*/
  assert(node);
  // handle objects
  if (node->properties) {
    for (size_t i = 0; i < node->property_count; i++) {
      int count = node->properties[i].property_count;
      for (int j = 0; j < count; j++) {
        if (node->properties[j].properties)
          jsonv_schema_free(node->properties[j].properties);
      }
    }
    free(node->properties);
  }
  // handle array
  if (node->items)
    jsonv_schema_free(node->items);
  free(node);
  /*#endregion*/
}
