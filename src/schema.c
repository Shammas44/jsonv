#include "schema.h"
#include "assert.h"
#include "compile.h"
#include "schema.handlers.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * Recursively compiles a section of the JSON Schema (defined by token_index)
 * into a SchemaNode C structure.
 */
Jsonv_SchemaNode *jsonv_compile_schema(const char *json,
                                       jsonv_tokiterator *it) {
  int token_index = jsonv_tokiterator_index(it);
  jsmntok_t *current_token = jsonv_tokiterator_current(it);

  if (token_index < 0 || current_token->type != JSMN_OBJECT) {
    return NULL;
  }

  Jsonv_SchemaNode *node =
      (Jsonv_SchemaNode *)calloc(1, sizeof(Jsonv_SchemaNode));
  if (node == NULL)
    return NULL;

  // Initialize defaults
  node->minLength = -1;
  node->maxLength = -1;
  node->type = jsonv_UNKNOWN;
  node->additional_properties = false;

  // --- Pass 1: Parse All Keywords and Recursively Compile Sub-Schemas ---
  for (int i = 0; i < current_token->size; i++) {
    // 1. IDENTIFY KEY and VALUE TOKENS
    jsmntok_t *key_token = jsonv_tokiterator_next(it);
    jsmntok_t *value_token = jsonv_tokiterator_next(it);

    // 2. ISOLATE THE JUMP: Calculate the index of the next KEY token.
    char key[100] = {0};
    int len = key_token->end - key_token->start;
    strncpy(key, json + key_token->start, len);

    typedef struct {
      char *key;
      jsonv_Schema_Handler handler;
    } K;

    jsonv_Schema_Context ctx = {.node = node,
                                .value = value_token,
                                .key = key_token,
                                .json = json,
                                .it = it};

    // ... [other keywords: $schema, $id, title, description, minLength,
    // maxLength, items, etc. are implicitly skipped here] ...
    // #define
    static K handlers[] = {{"type", jsonv_schema_handler_type},
                           {"properties", jsonv_schema_handler_properties},
                           {"items", jsonv_schema_handler_items},
                           {"required", jsonv_schema_handler_required}};
    for (unsigned long i = 0; i < (sizeof(handlers) / sizeof(handlers[0]));
         i++) {
      if (strcmp(key, handlers[i].key) == 0) {
        int e = handlers[i].handler(&ctx);
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
