#ifndef _JSONV_CTX_H_INCLUDED
#define _JSONV_CTX_H_INCLUDED
#include "arena.h"
#include "error.h"

typedef struct Jsonv_Context Jsonv_Context;

bool jsonv_ctx_prepare_data(Jsonv_Context **ctx, const unsigned char *json,
                            Arena *arena);
bool jsonv_ctx_prepare_schema(Jsonv_Context **ctx, const unsigned char *json,
                              Arena *arena);
void jsonv_ctx_print_data(Jsonv_Context *ctx);
void jsonv_ctx_print_schema(Jsonv_Context *ctx);
void jsonv_ctx_free(Jsonv_Context *ctx);
int jsonv_ctx_validate(Jsonv_Context *ctx);
Jsonv_error_stack *jsonv_ctx_errors(Jsonv_Context *ctx);

#endif
