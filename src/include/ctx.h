#ifndef _JSONV_CTX_H_INCLUDED
#define _JSONV_CTX_H_INCLUDED
#include "arena.h"
#include "value.h"
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

Jsonv_Schema* jsonv_schema_compile(
    Arena *schema_arena,
    const unsigned char *schema_json,
    const Jsonv_Config *config,
    E *out_error
);

Jsonv_Context* jsonv_ctx_create(
    Arena *execution_arena,
    const Jsonv_Config *config
);

bool jsonv_ctx_parse_data(
    Jsonv_Context *ctx,
    const unsigned char *data_json,
    Value *out_value
);

bool jsonv_ctx_validate(
    Jsonv_Context *ctx,
    const Jsonv_Schema *schema,
    Value data_value
);

const E* jsonv_ctx_get_error(const Jsonv_Context *ctx);

Arena* jsonv_ctx_arena(const Jsonv_Context *ctx);

void jsonv_ctx_reset(Jsonv_Context *ctx);

#endif
