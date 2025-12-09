#include "validate.h"
#include "assert.h"
#include "data.h"
#include "hint.h"
#include "schema.h"
#include "validate.handlers.h"
#include <string.h>

static char *data_type[] = {"string", "number", "integer", "boolean",
                            "object", "array",  "null",    "unknown"};

#define ERROR(format, ...)                                                     \
  char *p = jsonv_build_data_path(data, json_data);                                 \
  jsonv_path_reset(path);                                                      \
  JSONV_ENTER_FIELD(path, p);                                                  \
  JSONV_ERR(errors, path, format, __VA_ARGS__);                                \
  JSONV_LEAVE(path);

static void apply(void **x, void *cl) {
  /*#region*/
  ValidatorCtx *ctx = cl;
  Jsonv_Contraint *contraint = *(Jsonv_Contraint **)x;
  ctx->contraint = *x;
  contraint->fn(ctx);
  /*#endregion*/
}

int jsonv_validate(const char *json_schema, const Jsonv_SchemaNode *schema,
                   const char *json_data, const Jsonv_DataNode *data,
                   Jsonv_path *path, Jsonv_error_stack *errors) {
  assert(schema && data);
  int valid = 1;
  // 1. Check Type Match
  if (schema->type != data->type) {
    char *expected = data_type[schema->type];
    char *received = data_type[data->type];
    ERROR("Expected '%s', found data type '%s'.", expected, received);
    return 0;
  }

  // 2. Recursive structural checks (Object and Array)
  int required_present = 0;
  if (schema->type == jsonv_OBJECT) {
    // 2a. Check required properties and recurse on known properties
    char schema_key[100] = {0};
    HINT(json_schema, schema->key, schema_key);

    for (size_t i = 0; i < schema->property_count; i++) {
      const Jsonv_SchemaNode prop_schema = schema->properties[i];
      int found = 0;

      char schema_key[100] = {0};
      HINT(json_schema, prop_schema.key, schema_key);

      for (size_t j = 0; j < data->property_count; j++) {
        const Jsonv_DataNode *prop_data = data->properties[j];

        char data_key[100] = {0};
        HINT(json_data, prop_data->key, data_key);

        if (strcmp(data_key, schema_key) == 0) {
          found = 1;
          required_present++;

          if (!jsonv_validate(json_schema, &prop_schema, json_data, prop_data,
                              path, errors)) {
            valid = 0;
          }
          break;
        }
      }

      // Check for required fields
      if (prop_schema.required && !found) {
        ERROR("Required property '%s' is missing.", schema_key);
        valid = 0;
      }
    }
    // Check additional properties
    bool allow_more_props = schema->additional_properties;
    if (!allow_more_props && (data->property_count - required_present) > 0) {
      ERROR("Additional properties are not allowed.", schema_key);
      valid = 0;
    }

  } else if (schema->type == jsonv_ARRAY) {
    // 2b. Array validation: Check item count and recurse on all items
    for (size_t i = 0; i < data->property_count; i++) {
      if (!jsonv_validate(json_schema, schema->items, json_data,
                          data->properties[i], path, errors)) {
        valid = 0;
      }
    }
  }

  ValidatorCtx ctx = (ValidatorCtx){
      .errors = errors,
      .path = path,
      .json_schema = json_schema,
      .json_data = json_data,
      .data = data,
      .schema = schema,
  };

  list_map(schema->value_contraints, apply, &ctx);

  return valid;
}
