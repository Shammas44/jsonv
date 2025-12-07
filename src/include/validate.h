#ifndef _jsonv_UTILS_H_INCLUDED
#define _jsonv_UTILS_H_INCLUDED
#include "data.h"
#include "error.h"
#include "schema.h"

int jsonv_validate(const char *json_schema, const Jsonv_SchemaNode *schema,
                   const char *json_data, const Jsonv_DataNode *data,
                   Jsonv_path *path, Jsonv_error_stack *errors);

#endif
