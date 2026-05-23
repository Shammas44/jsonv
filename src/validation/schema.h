#ifndef _JSONV_SCHEMA_H_INCLUDED
#define _JSONV_SCHEMA_H_INCLUDED
#include "parser.h"

typedef struct SchemaRule SchemaRule;

SchemaRule *compile_schema(ASTNode *nodes, int root_idx, int *out_count);
void print_schema_rules(SchemaRule *rules, int count);

#endif
