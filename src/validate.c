#include "validate.h"
#include "assert.h"
#include "data.h"
#include "schema.h"
#include <stdio.h>
#include <string.h>

static char *data_type[] = {"string", "number", "integer", "boolean",
                            "object", "array",  "null",    "unknown"};

int jsonv_validate(const char *json_schema, const Jsonv_SchemaNode *schema,
                   const char *json_data, const Jsonv_DataNode *data,
                   Jsonv_path *path, Jsonv_error_stack *errors) {
  assert(schema && data);
  int valid = 1;
  // 1. Check Type Match
  if (schema->type != data->type) {
    char *expected = data_type[schema->type];
    char *received = data_type[data->type];
    char *p = jsonv_build_path(data, json_data);
    jsonv_path_reset(path);
    JSONV_ENTER_FIELD(path, p);
    JSONV_ERR(errors, path, "Expected '%s', found data type '%s'.", expected,
              received);
    JSONV_LEAVE(path);
    return 0;
  }
  // 2. Check Contraintes
  // 2. Check Children

  // 2. Recursive structural checks (Object and Array)
  int required_present = 0;
  if (schema->type == jsonv_OBJECT) {
    // 2a. Check required properties and recurse on known properties

    char schema_key[100] = {0};
    int schema_key_len = schema->key.end - schema->key.start;
    strncpy(schema_key, json_schema + schema->key.start, schema_key_len);
    for (size_t i = 0; i < schema->property_count; i++) {
      const Jsonv_SchemaNode prop_schema = schema->properties[i];
      int found = 0;

      char schema_key[100] = {0};
      int schema_key_len = prop_schema.key.end - prop_schema.key.start;
      strncpy(schema_key, json_schema + prop_schema.key.start, schema_key_len);

      for (size_t j = 0; j < data->property_count; j++) {
        const Jsonv_DataNode *prop_data = data->properties[j];

        char data_key[100] = {0};
        int data_key_len = prop_data->key.end - prop_data->key.start;
        strncpy(data_key, json_data + prop_data->key.start, data_key_len);

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
        char *p = jsonv_build_path(data, json_data);
        jsonv_path_reset(path);
        JSONV_ENTER_FIELD(path, p);
        JSONV_ERR(errors, path, "Required property '%s' is missing.",
                  schema_key);
        JSONV_LEAVE(path);
        valid = 0;
      }
    }
    // Check additional properties
    bool allow_more_props = schema->additional_properties;
    if (!allow_more_props && (data->property_count - required_present) > 0) {
      char *p = jsonv_build_path(data, json_data);
      jsonv_path_reset(path);
      JSONV_ENTER_FIELD(path, p);
      JSONV_ERR(errors, path, "Additional properties are not allowed.",
                schema_key);
      JSONV_LEAVE(path);
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
    // Check array constraints (e.g., minItems/maxItems)
    // if (schema->minItems > 0 &&
    //     data->childrenCount < (size_t)schema->minItems) {
    //   fprintf(stderr,
    //           "Validation Failed: Array size %zu is less than minItems
    //           %d.\n", data->childrenCount, schema->minItems);
    //   valid = 0;
    // }
  }

  return valid;
}
