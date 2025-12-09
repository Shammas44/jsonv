#ifndef _JSONV_COMPILE_H_INCLUDED
#define _JSONV_COMPILE_H_INCLUDED
#include "data.h"
#include "error.h"
#include "schema.h"
#include "token.h"

Jsonv_SchemaNode *jsonv_compile_schema(const char *json, jsonv_tokiterator *it,
                                       Jsonv_SchemaNode *parent,
                                       Jsonv_path *path,
                                       Jsonv_error_stack *errors);

Jsonv_DataNode *jsonv_compile_data(const char *json, jsonv_tokiterator *it,
                                   Jsonv_DataNode *parent, Jsonv_path *path,
                                   Jsonv_error_stack *errors);

#endif
