#ifndef _JSONV_CTX_H
#define _JSONV_CTX_H
#include "macro.h"
#include "arena.h"
#include "shape.h"
#include "validate.h"

typedef struct {
  size_t default_block_size;
  size_t max_limit;
  size_t shrink_at;
  size_t max_depth;
  size_t max_values;
  size_t max_objects;
  size_t max_array;
  size_t max_string_bytes;
} Jsonv_Config;

typedef struct Jsonv_Schema Jsonv_Schema;
typedef struct Jsonv_Context Jsonv_Context;

JSONV_API Jsonv_Schema* jsonv_schema_compile(
    Jsonv_Arena *schema_arena,
    const unsigned char *schema_json,
    const Jsonv_Config *config,
    E *out_error
);

JSONV_API Jsonv_Context* jsonv_ctx_create(
    Jsonv_Arena *execution_arena,
    const Jsonv_Config *config
);

JSONV_API bool jsonv_ctx_parse_data(
    Jsonv_Context *ctx,
    const unsigned char *data_json
);

JSONV_API bool jsonv_ctx_validate(
    Jsonv_Context *ctx,
    const Jsonv_Schema *schema
);

JSONV_API bool jsonv_ctx_get_value(
    Jsonv_Context *ctx,
    Jsonv_Value *out_value
);

JSONV_API const E* jsonv_ctx_get_error(const Jsonv_Context *ctx);

JSONV_API Jsonv_Arena* jsonv_ctx_arena(const Jsonv_Context *ctx);

JSONV_API void jsonv_ctx_reset(Jsonv_Context *ctx);

#endif
