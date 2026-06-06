#ifndef _JSONV_SCHEMA_H_INCLUDED
#define _JSONV_SCHEMA_H_INCLUDED
#include "parser.h"
#include "arena.h"
#include <stdint.h>

typedef enum {
  TYPE_NULL = 1 << 0,
  TYPE_BOOL = 1 << 1,
  TYPE_NUMBER = 1 << 2,
  TYPE_STRING = 1 << 3,
  TYPE_ARRAY = 1 << 4,
  TYPE_OBJECT = 1 << 5,
  TYPE_INTEGER = 1 << 6
} SchemaTypeMask;

typedef enum {
  OP_END = 0,
  OP_FAIL,
  OP_TYPE,
  OP_MINIMUM,
  OP_MAXIMUM,
  OP_MIN_LENGTH,
  OP_MAX_LENGTH,
  OP_MIN_ITEMS,
  OP_MAX_ITEMS,
  OP_ITEMS,
  OP_REQUIRED,
  OP_PROPERTIES,
  OP_MULTIPLE_OF,
  OP_EXCLUSIVE_MINIMUM,
  OP_EXCLUSIVE_MAXIMUM,
  OP_PATTERN,
  OP_MIN_PROPERTIES,
  OP_MAX_PROPERTIES,
  OP_UNIQUE_ITEMS,
  OP_CONTAINS,
  OP_NOT,
  OP_ALL_OF,
  OP_ANY_OF,
  OP_ONE_OF,
  OP_IF_THEN_ELSE,
  OP_PROPERTY_NAMES
} Opcode;

uint8_t *compile_schema(Jsonv_Arena *arena, ASTNode *nodes, int ast_count, int root_idx, int *out_length);

#endif
