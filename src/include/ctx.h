#ifndef _JSONV_CTX_H_INCLUDED
#define _JSONV_CTX_H_INCLUDED
#include "arena.h"
#include "validate.h"

typedef struct {
  // Arena
  size_t default_block_size;
  size_t max_limit;
  size_t shrink_at;
  // Prescan
  size_t max_depth;
  size_t max_values;
  size_t max_objects;
  size_t max_array;
  size_t max_string_bytes;
} Jsonv_Ctx_Config;

typedef struct Jsonv_Context Jsonv_Context;

bool jsonv_ctx_init(Jsonv_Context **ctx, Arena* schema_arena, Jsonv_Ctx_Config *config);
bool jsonv_ctx_prepare_data(Jsonv_Context **ctx, const unsigned char *json);
bool jsonv_ctx_prepare_schema(Jsonv_Context **ctx, const unsigned char *json);
int  jsonv_ctx_validate(Jsonv_Context *ctx);
void jsonv_ctx_print_data(Jsonv_Context *ctx);
void jsonv_ctx_print_schema(Jsonv_Context *ctx);
void jsonv_ctx_free(Jsonv_Context *ctx);
E *  jsonv_ctx_error(Jsonv_Context *ctx);

#endif
