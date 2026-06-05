#include "validate.h"
#include "shape.internal.h"
#include "schema.h"
#include "ctx.h"
#include "parser.h"
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


/* Helper: Compares raw key view from AST against Schema rule Token */
static bool key_matches_token_ast(const char *k_start, size_t k_len, Token t) {
  /*#region*/
  if (t.type != T_STRING) return false;
  
  const char *t_start = (const char *)t.value.string.start;
  size_t t_len = t.value.string.length;
  
  // Strip quotes if they exist in schema representation
  if (t_len >= 2 && t_start[0] == '"' && t_start[t_len - 1] == '"') {
    t_start++;
    t_len -= 2;
  }
  
  if (k_len != t_len) return false;
  return memcmp(k_start, t_start, t_len) == 0;
  /*#endregion*/
}

/* Recursive validation loop on ASTNode pool index */
bool validate_ast(
    Jsonv_Context *ctx,
    ASTNode *pool,
    const Jsonv_Schema *schema,
    int rule_idx,
    int node_idx,
    const char *path,
    E *out_err
) {
  /*#region*/
  if (rule_idx < 0 || rule_idx >= schema->rule_count) return true;
  const SchemaRule *r = &schema->rules[rule_idx];
  ASTNode *node = &pool[node_idx];

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
    if (node->type == AST_OBJECT) {
      match = (r->type_mask & TYPE_OBJECT);
    } else if (node->type == AST_ARRAY) {
      match = (r->type_mask & TYPE_ARRAY);
    } else if (node->type == AST_LEAF) {
      switch (node->token.type) {
        case T_NULL:
          match = (r->type_mask & TYPE_NULL);
          break;
        case T_TRUE:
        case T_FALSE:
          match = (r->type_mask & TYPE_BOOL);
          break;
        case T_NUMBER:
          match = (r->type_mask & TYPE_NUMBER) || 
                  ((r->type_mask & TYPE_INTEGER) && (node->token.value.number == (int64_t)node->token.value.number));
          break;
        case T_STRING:
          match = (r->type_mask & TYPE_STRING);
          break;
        default:
          break;
      }
    }
    
    if (!match) {
      out_err->type = Jsonv_Type_error;
      out_err->path = path;
      snprintf(out_err->description, sizeof(out_err->description), "Type mismatch.");
      return false;
    }
  }

  // 3. Numeric Constraints
  if (node->type == AST_LEAF && node->token.type == T_NUMBER) {
    double num = node->token.value.number;
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
  if (node->type == AST_LEAF && node->token.type == T_STRING) {
    size_t len = node->token.value.string.length;
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
  if (node->type == AST_ARRAY) {
    // Count items
    int count = 0;
    int child_idx = node->first_child;
    while (child_idx != -1) {
      count++;
      child_idx = pool[child_idx].next_sibling;
    }

    if (r->min_items >= 0 && count < r->min_items) {
      out_err->type = Jsonv_MinItems_error;
      out_err->path = path;
      snprintf(out_err->description, sizeof(out_err->description), "Array has too few items, expected >= %d, got %d.", r->min_items, count);
      return false;
    }
    if (r->max_items >= 0 && count > r->max_items) {
      out_err->type = Jsonv_MinItems_error;
      out_err->path = path;
      snprintf(out_err->description, sizeof(out_err->description), "Array has too many items, expected <= %d, got %d.", r->max_items, count);
      return false;
    }

    // Validate child elements
    if (r->items_rule >= 0) {
      child_idx = node->first_child;
      for (int i = 0; child_idx != -1; i++) {
        char item_path[64];
        snprintf(item_path, sizeof(item_path), "%s[%d]", path, i);
        char *arena_path = (char *)jsonv_arena_alloc(jsonv_ctx_arena(ctx), strlen(item_path) + 1);
        if (arena_path) {
          strcpy(arena_path, item_path);
        }
        if (!validate_ast(ctx, pool, schema, r->items_rule, child_idx, arena_path ? arena_path : path, out_err)) {
          return false;
        }
        child_idx = pool[child_idx].next_sibling;
      }
    }
  }

  // 6. Object Constraints
  if (node->type == AST_OBJECT) {
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
      
      int val_idx = find_property(pool, node_idx, req_key);
      if (val_idx == -1) {
        out_err->type = Jsonv_Required_error;
        out_err->path = path;
        snprintf(out_err->description, sizeof(out_err->description), "Missing required field '%.*s'.", (int)t_len, t_start);
        return false;
      }
    }

    // B. Check Properties and AdditionalProperties
    int curr = node->first_child;
    while (curr != -1) {
      if (pool[curr].type != AST_SKIPPED) {
        Token k = pool[curr].token;
        int val_idx = pool[curr].next_sibling;
        
        // Extract key string view
        const char *k_start = (const char *)k.value.string.start;
        size_t k_len = k.value.string.length;
        
        // Match against properties in the SchemaRule
        bool matched = false;
        int prop_rule_idx = -1;
        for (int p = 0; p < r->prop_count; p++) {
          if (key_matches_token_ast(k_start, k_len, r->props[p].key)) {
            matched = true;
            prop_rule_idx = r->props[p].rule_index;
            break;
          }
        }
        
        // Construct new path for validation errors
        size_t p_len = strlen(path);
        size_t needed = p_len + k_len + 2;
        char *new_path = (char *)jsonv_arena_alloc(jsonv_ctx_arena(ctx), needed);
        if (new_path) {
          if (p_len > 0) {
            snprintf(new_path, needed, "%s.%.*s", path, (int)k_len, k_start);
          } else {
            snprintf(new_path, needed, "%.*s", (int)k_len, k_start);
          }
        }
        
        if (matched) {
          if (val_idx != -1) {
            if (!validate_ast(ctx, pool, schema, prop_rule_idx, val_idx, new_path ? new_path : path, out_err)) {
              return false;
            }
          }
        } else {
          // Check additional properties rule
          if (r->additional_props_rule == -2) {
            out_err->type = Jsonv_AdditionalProperties_error;
            out_err->path = new_path ? new_path : path;
            snprintf(out_err->description, sizeof(out_err->description), "Additional property not allowed.");
            return false;
          } else if (r->additional_props_rule >= 0) {
            if (val_idx != -1) {
              if (!validate_ast(ctx, pool, schema, r->additional_props_rule, val_idx, new_path ? new_path : path, out_err)) {
                return false;
              }
            }
          }
        }
      }
      
      int val_idx = pool[curr].next_sibling;
      if (val_idx == -1) break;
      curr = pool[val_idx].next_sibling;
    }
  }

  return true;
  /*#endregion*/
}
