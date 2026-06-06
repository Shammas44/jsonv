#ifndef _jsonv_UTILS_H_INCLUDED
#define _jsonv_UTILS_H_INCLUDED

#include "except.h"
#include "stack.h"
#include "shape.h"
#include <stdbool.h>
#include <stdint.h>

#define JSONV_SCHEMA_IS_VALID -1

typedef struct {
  char description[100];
  const char *path; // Arena-allocated or zero-copy read-only view
  Jsonv_Except_Type type;
} E;

typedef struct Jsonv_Context Jsonv_Context;
typedef struct ASTNode ASTNode;
struct Jsonv_Schema {
  uint8_t *bytecode;
  uint32_t length;
};
typedef struct Jsonv_Schema Jsonv_Schema;

bool validate_bytecode(
    Jsonv_Context *ctx,
    ASTNode *pool,
    const Jsonv_Schema *schema,
    uint32_t offset,
    int node_idx,
    const char *path,
    E *out_err
);

#endif
