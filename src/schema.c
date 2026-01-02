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

Jsonv_SchemaNode *jsonv_compile_schema(const char *json, jsonv_tokiterator *it,
                                       Jsonv_SchemaNode *parent,
                                       Jsonv_path *path,
                                       Jsonv_error_stack *errors){
  /*#region*/
  (void)(json);
  (void)(it);
  (void)(parent);
  (void)(path);
  (void)(errors);
  return NULL;
  // (void)(parent);
  // int token_index = jsonv_tokiterator_index(it);
  // jsmntok_t *current_token = jsonv_tokiterator_current(it);

  // if (token_index < 0 || current_token->type != JSMN_OBJECT) {
  //   return NULL;
  // }

  // Jsonv_SchemaNode *node = CALLOC(1, sizeof(Jsonv_SchemaNode));

  // // Initialize defaults
  // node->type = jsonv_UNKNOWN;
  // node->additional_properties = false;
  // node->parent = parent;
  // char bff[100] = {0};
  // if(parent != NULL){
  // jsmntok_t *key_token = jsonv_tokiterator_relative(it, -1);
  // TOK(json, *key_token, bff);
  // }

  // // --- Pass 1: Parse All Keywords and Recursively Compile Sub-Schemas ---
  // for (int i = 0; i < current_token->size; i++) {
  //   // 1. IDENTIFY KEY and VALUE TOKENS
  //   jsmntok_t *key_token = jsonv_tokiterator_next(it);
  //   jsmntok_t *value_token = jsonv_tokiterator_next(it);

  //   // 2. ISOLATE THE JUMP: Calculate the index of the next KEY token.
  //   char key[100] = {0};
  //   TOK(json, *key_token, key);

  //   jsonv_Schema_Context ctx = {.schema = node,
  //                               .value = value_token,
  //                               .key = key_token,
  //                               .json = json,
  //                               .it = it,.path=path,.errors=errors
  //   };

  //   for (unsigned long j = 0; j < g_handlers_length; j++) {
  //     if (strcmp(key, g_handlers[j].key) == 0) {
  //       int e = g_handlers[j].handler(&ctx);
  //       (void)(e);
  //       //TODO change this
  //       // assert(!e);
  //       // break;
  //     }
  //   }
  // }

  // // --- Pass 2: Merge Required Flags into Properties (Unchanged but validated)
  // char **required_keys = node->required_keys;
  // size_t required_count = node->required_keys_length;
  // if (node->properties && required_keys) {
  //   for (size_t k = 0; k < required_count; k++) {
  //     for (size_t p = 0; p < node->property_count; p++) {
  //       // TODO improve this
  //       if (required_keys[k] != NULL && node->properties[p].key.start != 0) {
  //         int len = node->properties[p].key.end - node->properties[p].key.start;
  //         if (strncmp(required_keys[k], json + node->properties[p].key.start,
  //                     len) == 0) {
  //           node->properties[p].required = true;
  //           break;
  //         }
  //       }
  //     }
  //   }
  // }

  // // Clean up temporary required keys array
  // if (required_keys) {
  //   free(required_keys);
  // }
  // return node;
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

/*
 * Build JSON path for a node:
 *   object members → ".key"
 *   array members  → "[index]"
 *
 * Returned string must be free()'d by the caller.
 */
char *jsonv_build_schema_path(const Jsonv_SchemaNode *node, const char *json) {
  if (!node)
    return strdup("$");

  char **segments = NULL;
  size_t seg_count = 0;

  const Jsonv_SchemaNode *cur = node;

  // Traverse from the current node (cur) up to the root (parent == NULL)
  while (cur->parent != NULL) {
    const Jsonv_SchemaNode *parent = cur->parent;
    char buffer[256];
    char *segment_value = NULL; 

    // Find the relationship of 'cur' to 'parent'
    
    // 1. If 'cur' is the single 'items' schema for a list validation.
    // This is typically not represented as an index in the path, but let's handle it.
    if (parent->type == jsonv_ARRAY && parent->items == cur) {
        // Path should typically represent the element index [N] or [*] if general
        // Since we don't know the instance index, we use a placeholder or assume [0]
        segment_value = strdup("[?]");
    } 
    // 2. If 'cur' is one of the schemas in parent->properties (used for both object properties and tuple array items)
    else {
        // Search through parent->properties to identify 'cur' and determine the segment format
        bool found = false;
        
        for (size_t i = 0; i < parent->property_count; ++i) {
            if (&parent->properties[i] == cur) {
                found = true;
                
                if (parent->type == jsonv_OBJECT) {
                    // Object Property: use the key from 'cur' and format as .key
                    // Note: cur->key is the property name in the schema
                    int len = cur->key.end - cur->key.start;
                    if (len > 0) {
                        char *key_str = malloc(len + 1); 
                        strncpy(key_str, json + cur->key.start, len);
                        key_str[len] = '\0';
                        
                        // Format: .key (e.g., .product)
                        snprintf(buffer, sizeof(buffer), ".%s", key_str);
                        free(key_str);
                        segment_value = strdup(buffer);
                    } else {
                        segment_value = strdup(".?");
                    }
                } else if (parent->type == jsonv_ARRAY) {
                    // Array Item (Tuple): use the index 'i' and format as [index]
                    // Format: [index] (e.g., [2])
                    snprintf(buffer, sizeof(buffer), "[%zu]", i);
                    segment_value = strdup(buffer);
                }
                
                break;
            }
        }
        
        // Fallback for an unknown relationship
        if (!found) {
            segment_value = strdup(".?"); 
        }
    }


    // Push segment
    segments = realloc(segments, sizeof(char *) * (seg_count + 1));
    segments[seg_count++] = segment_value;

    cur = parent;
  }
  
  // --- Path Reconstruction ---
  
  // 1. Compute final length
  size_t length = 2; // for `$` and '\0'
  for (size_t i = 0; i < seg_count; i++)
    length += strlen(segments[i]);

  char *path = malloc(length);
  strcpy(path, "$");

  // 2. Add segments reversed (from root to leaf)
  for (size_t i = 0; i < seg_count; i++) {
    char *segment = segments[seg_count - 1 - i];
    
    // Check if it's the very first segment after '$' and it starts with a '.'
    // (e.g., the first property of the root object: e.g., turning "$.product" into "$product" or "$properties").
    // We strictly use '$' as the root indicator, and then append the segments.
    // The key is to skip the *first* leading dot from the *first* property name after '$'.
    if (i == 0 && segment[0] == '.') {
      // Append the segment starting from the second character (skipping the '.')
      strcat(path, segment + 1); 
    } else {
      // Append the rest of the segments as is (they will be like .key or [index])
      strcat(path, segment);
    }
  }

  // 3. Cleanup
  for (size_t i = 0; i < seg_count; i++)
    free(segments[i]);
  free(segments);

  return path;
}
