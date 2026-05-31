#include "validate.h"
#include "shape.internal.h"
#include "schema.h"
#include "ctx.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Forward declaration of the schema rule struct and internal compile definitions */
typedef enum {
  TYPE_NULL = 1 << 0,
  TYPE_BOOL = 1 << 1,
  TYPE_NUMBER = 1 << 2,
  TYPE_STRING = 1 << 3,
  TYPE_ARRAY = 1 << 4,
  TYPE_OBJECT = 1 << 5,
  TYPE_INTEGER = 1 << 6
} SchemaTypeMask;

typedef struct {
  Token key;
  int rule_index;
} PropertyRule;

struct SchemaRule {
  int type_mask;
  int next_rule;
  double min;
  double max;
  bool has_min;
  bool has_max;
  int min_len;
  int max_len;
  int items_rule;
  int min_items;
  int max_items;
  PropertyRule *props;
  int prop_count;
  Token *required;
  int required_count;
  int additional_props_rule;
};

struct Jsonv_Schema {
  SchemaRule *rules;
  int rule_count;
};

/* Helper: Compares a length-prefixed key string view against a token string view */
static bool key_matches_token(const_lstr_t key, Token t) {
  /*#region*/
  if (t.type != T_STRING) return false;
  
  const char *t_start = (const char *)t.value.string.start;
  size_t t_len = t.value.string.length;
  
  // Normalize quotes if necessary
  if (t_len >= 2 && t_start[0] == '"' && t_start[t_len - 1] == '"') {
    t_start++;
    t_len -= 2;
  }
  
  size_t k_len = ((const StringHeader *)key - 1)->length;
  if (k_len != t_len) return false;
  
  return memcmp(key, t_start, t_len) == 0;
  /*#endregion*/
}

/* Recursive validation loop on transient Value structure */
bool validate_value(
    Jsonv_Context *ctx,
    const Jsonv_Schema *schema,
    int rule_idx,
    Value val,
    const char *path,
    E *out_err
) {
  /*#region*/
  if (rule_idx < 0 || rule_idx >= schema->rule_count) return true;
  const SchemaRule *r = &schema->rules[rule_idx];

  // 1. SC_FALSE / Always Fail case
  if (r->type_mask == -1) {
    out_err->type = Jsonv_ValueNotAllowed_error;
    out_err->path = path;
    snprintf(out_err->description, sizeof(out_err->description), "Value not allowed (Schema is false).");
    return false;
  }

  // 2. Type Mask Check
  if (r->type_mask > 0) {
    bool match = false;
    switch (val.tag) {
      case VAL_NULL:
        match = (r->type_mask & TYPE_NULL);
        break;
      case VAL_BOOLEAN:
        match = (r->type_mask & TYPE_BOOL);
        break;
      case VAL_INT:
        match = (r->type_mask & TYPE_NUMBER) || (r->type_mask & TYPE_INTEGER);
        break;
      case VAL_DOUBLE:
        match = (r->type_mask & TYPE_NUMBER);
        if (match && (r->type_mask & TYPE_INTEGER)) {
          // Double must be integer value
          match = (val.as.d == (int64_t)val.as.d);
        }
        break;
      case VAL_STRING:
        match = (r->type_mask & TYPE_STRING);
        break;
      case VAL_ARRAY:
        match = (r->type_mask & TYPE_ARRAY);
        break;
      case VAL_OBJ:
        match = (r->type_mask & TYPE_OBJECT);
        break;
      default:
        break;
    }
    
    if (!match) {
      out_err->type = Jsonv_Type_error;
      out_err->path = path;
      snprintf(out_err->description, sizeof(out_err->description), "Type mismatch.");
      return false;
    }
  }

  // 3. Number Constraints
  if (val.tag == VAL_INT || val.tag == VAL_DOUBLE) {
    double num = (val.tag == VAL_INT) ? (double)val.as.i : val.as.d;
    if (r->has_min && num < r->min) {
      out_err->type = Jsonv_Minimum_error;
      out_err->path = path;
      snprintf(out_err->description, sizeof(out_err->description), "Value too small, expected >= %.2f, got %.2f.", r->min, num);
      return false;
    }
    if (r->has_max && num > r->max) {
      out_err->type = Jsonv_Maximum_error;
      out_err->path = path;
      snprintf(out_err->description, sizeof(out_err->description), "Value too large, expected <= %.2f, got %.2f.", r->max, num);
      return false;
    }
  }

  // 4. String Constraints
  if (val.tag == VAL_STRING) {
    size_t len = val_str_len(val);
    if (r->min_len >= 0 && len < (size_t)r->min_len) {
      out_err->type = Jsonv_MinLength_error;
      out_err->path = path;
      snprintf(out_err->description, sizeof(out_err->description), "String too short, expected >= %d, got %lu.", r->min_len, len);
      return false;
    }
    if (r->max_len >= 0 && len > (size_t)r->max_len) {
      out_err->type = Jsonv_MaxLength_error;
      out_err->path = path;
      snprintf(out_err->description, sizeof(out_err->description), "String too long, expected <= %d, got %lu.", r->max_len, len);
      return false;
    }
  }

  // 5. Array Constraints
  if (val.tag == VAL_ARRAY && r->items_rule >= 0) {
    Arr *arr = (Arr *)val.as.p;
    for (int i = 0; i < arr->length; i++) {
      char item_path[64];
      snprintf(item_path, sizeof(item_path), "%s[%d]", path, i);
      char *arena_path = (char *)jsonv_arena_alloc(jsonv_ctx_arena(ctx), strlen(item_path) + 1);
      if (arena_path) {
        strcpy(arena_path, item_path);
      }
      if (!validate_value(ctx, schema, r->items_rule, arr->items[i], arena_path ? arena_path : path, out_err)) {
        return false;
      }
    }
  }

  // 6. Object Constraints
  if (val.tag == VAL_OBJ) {
    Obj *obj = (Obj *)val.as.p;
    
    // A. Check Required Properties
    for (int i = 0; i < r->required_count; i++) {
      Token t = r->required[i];
      const char *t_start = (const char *)t.value.string.start;
      size_t t_len = t.value.string.length;
      if (t_len >= 2 && t_start[0] == '"' && t_start[t_len - 1] == '"') {
        t_start++;
        t_len -= 2;
      }
      
      // Make a temporary null-terminated string to look up
      char *req_key = (char *)jsonv_arena_alloc(jsonv_ctx_arena(ctx), t_len + 1);
      if (!req_key) return false;
      memcpy(req_key, t_start, t_len);
      req_key[t_len] = '\0';
      
      // Construct length-prefixed StringHeader to match
      size_t total_size = sizeof(StringHeader) + t_len + 1;
      StringHeader *header = (StringHeader *)jsonv_arena_alloc(jsonv_ctx_arena(ctx), total_size);
      if (!header) return false;
      header->length = (uint32_t)t_len;
      memcpy(header->data, t_start, t_len);
      header->data[t_len] = '\0';
      const_lstr_t lstr_key = (const_lstr_t)header->data;
      
      Value dummy;
      if (!obj_get(obj, lstr_key, &dummy)) {
        out_err->type = Jsonv_Required_error;
        out_err->path = path;
        snprintf(out_err->description, sizeof(out_err->description), "Missing required field '%.*s'.", (int)t_len, t_start);
        return false;
      }
    }

    // B. Check Properties and AdditionalProperties
    if (obj->shape) {
      for (int slot = 0; slot < obj->shape->slot_count; slot++) {
        const_lstr_t key = shape_get_key_at(obj->shape, slot);
        Value p_val = obj->slots[slot];
        
        bool matched = false;
        int prop_rule_idx = -1;
        for (int k = 0; k < r->prop_count; k++) {
          if (key_matches_token(key, r->props[k].key)) {
            matched = true;
            prop_rule_idx = r->props[k].rule_index;
            break;
          }
        }
        
        // Construct new path
        size_t p_len = strlen(path);
        size_t k_len = ((const StringHeader *)key - 1)->length;
        size_t needed = p_len + k_len + 2;
        char *new_path = (char *)jsonv_arena_alloc(jsonv_ctx_arena(ctx), needed);
        if (new_path) {
          if (p_len > 0) {
            snprintf(new_path, needed, "%s.%.*s", path, (int)k_len, key);
          } else {
            snprintf(new_path, needed, "%.*s", (int)k_len, key);
          }
        }
        
        if (matched) {
          if (!validate_value(ctx, schema, prop_rule_idx, p_val, new_path ? new_path : path, out_err)) {
            return false;
          }
        } else {
          // Check additional properties rule
          if (r->additional_props_rule == -2) {
            out_err->type = Jsonv_AdditionalProperties_error;
            out_err->path = new_path ? new_path : path;
            snprintf(out_err->description, sizeof(out_err->description), "Additional property not allowed.");
            return false;
          } else if (r->additional_props_rule >= 0) {
            if (!validate_value(ctx, schema, r->additional_props_rule, p_val, new_path ? new_path : path, out_err)) {
              return false;
            }
          }
        }
      }
    }
  }

  return true;
  /*#endregion*/
}
