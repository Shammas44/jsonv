#ifndef _JSONV_UTILS_H
#define _JSONV_UTILS_H

#include "error.h"
#include <stdbool.h>
#include <stdint.h>

#define JSONV_SCHEMA_IS_VALID -1

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
    Jsonv_Error *out_err
);

#endif
