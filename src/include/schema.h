#ifndef _JSONV_SCHEMA_H_INCLUDED
#define _JSONV_SCHEMA_H_INCLUDED
#include "hint.h"
#include "list.h"
#include "type.h"
#include "jsmn.h"
#include <stdbool.h>
#include <stdio.h>

typedef int (*jsonv_Validate_Handler)(void *);

typedef struct Jsonv_SchemaNode Jsonv_SchemaNode;

typedef struct Jsonv_Contraint {
  jsonv_Hint name;
  jsonv_Hint value;
  jsonv_Validate_Handler fn;
} Jsonv_Contraint;

typedef struct Jsonv_SchemaNode {
  jsonv_t type;
  jsonv_Hint key;
  Jsonv_SchemaNode *parent;
  // --- Constraints ---
  List value_contraints;
  bool required;
  bool additional_properties;
  // --- For OBJECT/ARRAY ---
  Jsonv_SchemaNode *properties;
  size_t property_count;
  char **required_keys;
  size_t required_keys_length;
  Jsonv_SchemaNode *items;
  // --- References ---
  // In a full implementation, you'd store $ref information here,
} Jsonv_SchemaNode;

// Frees the memory allocated for a compiled schema node.
void jsonv_schema_free(Jsonv_SchemaNode *node);
char *jsonv_build_schema_path(const Jsonv_SchemaNode *node, const char *json);

#endif
