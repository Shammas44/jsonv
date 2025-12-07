#ifndef _JSONV_CTX_H_INCLUDED
#define _JSONV_CTX_H_INCLUDED
#include "error.h"
#include <jsmn/jsmn.h>

typedef struct Jsonv_Context Jsonv_Context;

int jsonv_ctx_prepare_data(Jsonv_Context **ctx, const char *json_schema, jsmntok_t**tok);
int jsonv_ctx_prepare_schema(Jsonv_Context **ctx, const char *json_schema,jsmntok_t**tok );
void jsonv_ctx_print_data(Jsonv_Context *ctx, const char*json);
void jsonv_ctx_print_schema(Jsonv_Context *ctx, const char*json);
void jsonv_ctx_free(Jsonv_Context *ctx);
int jsonv_ctx_validate(Jsonv_Context *ctx, const char*schema, const char*data);
Jsonv_error_stack *jsonv_ctx_errors(Jsonv_Context *ctx);

#endif
