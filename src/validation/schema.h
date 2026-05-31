#ifndef _JSONV_SCHEMA_H_INCLUDED
#define _JSONV_SCHEMA_H_INCLUDED
#include "parser.h"
#include "arena.h"

typedef struct SchemaRule SchemaRule;

SchemaRule *compile_schema(Jsonv_Arena *arena, ASTNode *nodes, int ast_count, int root_idx, int *out_count);
void print_schema_rules(SchemaRule *rules, int count);

#endif
