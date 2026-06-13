#ifndef _JSONV_ERROR_H
#define _JSONV_ERROR_H

typedef enum {
  // number
  Jsonv_Maximum_error,
  Jsonv_Minimum_error,
  Jsonv_MultipleOf_error,
  Jsonv_ExclusiveMaximum_error,
  Jsonv_ExclusiveMinimum_error,
  // string
  Jsonv_MaxLength_error,
  Jsonv_MinLength_error,
  Jsonv_Pattern_error,
  // object
  Jsonv_Type_error,
  Jsonv_AdditionalProperties_error,
  Jsonv_MinProperties_error,
  Jsonv_MaxProperties_error,
  // array
  Jsonv_MinItems_error,
  Jsonv_Required_error,
  Jsonv_UniqueItems_error,
  Jsonv_Contains_error,
  Jsonv_Not_error,
  Jsonv_AllOf_error,
  Jsonv_AnyOf_error,
  Jsonv_OneOf_error,
  Jsonv_IfThenElse_error,
  Jsonv_Format_error,
  // else
  Jsonv_ValueNotAllowed_error,
  Jsonv_Malformed_json,
  Jsonv_Maximum_Token_Bytes_Reached,
  Jsonv_Maximum_Object_Reached,
  Jsonv_Maximum_Array_Reached,
  Jsonv_Maximum_Values_Reached,
  Jsonv_Mem_Failed,
  Jsonv_Arena_Limit_Reached,
  Jsonv_Maximum_Nested_Depth_Reached,
  Jsonv_Assertion_Failed,
  Jsonv_Compile_Regexp_Failed
} Jsonv_Except_Type;

typedef struct {
  char description[100];
  const char *path; // Arena-allocated or zero-copy read-only view
  Jsonv_Except_Type type;
} Jsonv_Error;

#endif
