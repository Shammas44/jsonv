#ifndef _JSONV_VALIDATE_HANDLERS_H_INCLUDED
#define _JSONV_VALIDATE_HANDLERS_H_INCLUDED
#include "data.h"
#include "error.h"
#include "schema.h"

typedef struct ValidatorCtx {
  Jsonv_Contraint *contraint;
  const Jsonv_SchemaNode *schema;
  const Jsonv_DataNode *data;
  const char *json_data;
  const char *json_schema;
  Jsonv_path *path;
  Jsonv_error_stack *errors;
} ValidatorCtx;

#endif
