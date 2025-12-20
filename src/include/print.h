#ifndef _JSONV_PRINT_H_INCLUDED
#define _JSONV_PRINT_H_INCLUDED
#include "data.h"
#include "node.h"
#include "schema.h"

void jsonv_print_schema_internal(const Jsonv_SchemaNode *schema, int depth,
                                 const char *json);
void jsonv_print_data_internal(const Jsonv_DataNode *node, int depth,
                               const char *json);
void jsonv_print_node_internal(const Jsonv_Node *node, int depth,
                               const char *json);
#endif
