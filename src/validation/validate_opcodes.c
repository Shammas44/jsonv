#include "validate_internal.h"

static bool handle_fail(Jsonv_Context *ctx, ASTNode *pool, const Jsonv_Schema *schema, const uint8_t **pc, int node_idx, const char *path, E *out_err, const uint8_t *constant_pool) {
  /*#region*/
  (void)ctx; (void)pool; (void)schema; (void)pc; (void)node_idx; (void)constant_pool;
  out_err->type = Jsonv_ValueNotAllowed_error;
  out_err->path = path;
  snprintf(out_err->description, sizeof(out_err->description), "Value not allowed (Schema is false).");
  return false;
  /*#endregion*/
}

static bool handle_type(Jsonv_Context *ctx, ASTNode *pool, const Jsonv_Schema *schema, const uint8_t **pc, int node_idx, const char *path, E *out_err, const uint8_t *constant_pool) {
  /*#region*/
  (void)ctx; (void)schema; (void)constant_pool;
  uint32_t type_mask = read_uint32(pc);
  ASTNode *node = &pool[node_idx];
  bool match = false;
  if (node->type == AST_OBJECT) {
    match = (type_mask & TYPE_OBJECT);
  } else if (node->type == AST_ARRAY) {
    match = (type_mask & TYPE_ARRAY);
  } else if (node->type == AST_LEAF) {
    switch (node->token.type) {
      case T_NULL:
        match = (type_mask & TYPE_NULL);
        break;
      case T_TRUE:
      case T_FALSE:
        match = (type_mask & TYPE_BOOL);
        break;
      case T_NUMBER:
        match = (type_mask & TYPE_NUMBER) || 
                ((type_mask & TYPE_INTEGER) && (node->token.value.number == (int64_t)node->token.value.number));
        break;
      case T_STRING:
        match = (type_mask & TYPE_STRING);
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
  return true;
  /*#endregion*/
}

static bool handle_minimum(Jsonv_Context *ctx, ASTNode *pool, const Jsonv_Schema *schema, const uint8_t **pc, int node_idx, const char *path, E *out_err, const uint8_t *constant_pool) {
  /*#region*/
  (void)ctx; (void)schema; (void)constant_pool;
  double min_val = read_double(pc);
  ASTNode *node = &pool[node_idx];
  if (node->type == AST_LEAF && node->token.type == T_NUMBER) {
    double num = node->token.value.number;
    if (num < min_val) {
      out_err->type = Jsonv_Minimum_error;
      out_err->path = path;
      snprintf(out_err->description, sizeof(out_err->description), "Value too small, expected >= %.2f, got %.2f.", min_val, num);
      return false;
    }
  }
  return true;
  /*#endregion*/
}

static bool handle_maximum(Jsonv_Context *ctx, ASTNode *pool, const Jsonv_Schema *schema, const uint8_t **pc, int node_idx, const char *path, E *out_err, const uint8_t *constant_pool) {
  /*#region*/
  (void)ctx; (void)schema; (void)constant_pool;
  double max_val = read_double(pc);
  ASTNode *node = &pool[node_idx];
  if (node->type == AST_LEAF && node->token.type == T_NUMBER) {
    double num = node->token.value.number;
    if (num > max_val) {
      out_err->type = Jsonv_Maximum_error;
      out_err->path = path;
      snprintf(out_err->description, sizeof(out_err->description), "Value too large, expected <= %.2f, got %.2f.", max_val, num);
      return false;
    }
  }
  return true;
  /*#endregion*/
}

static bool handle_min_length(Jsonv_Context *ctx, ASTNode *pool, const Jsonv_Schema *schema, const uint8_t **pc, int node_idx, const char *path, E *out_err, const uint8_t *constant_pool) {
  /*#region*/
  (void)ctx; (void)schema; (void)constant_pool;
  int32_t min_len = read_int32(pc);
  ASTNode *node = &pool[node_idx];
  if (node->type == AST_LEAF && node->token.type == T_STRING) {
    size_t len = node->token.value.string.length;
    if (len < (size_t)min_len) {
      out_err->type = Jsonv_MinLength_error;
      out_err->path = path;
      snprintf(out_err->description, sizeof(out_err->description), "String too short, expected >= %d, got %lu.", min_len, len);
      return false;
    }
  }
  return true;
  /*#endregion*/
}

static bool handle_max_length(Jsonv_Context *ctx, ASTNode *pool, const Jsonv_Schema *schema, const uint8_t **pc, int node_idx, const char *path, E *out_err, const uint8_t *constant_pool) {
  /*#region*/
  (void)ctx; (void)schema; (void)constant_pool;
  int32_t max_len = read_int32(pc);
  ASTNode *node = &pool[node_idx];
  if (node->type == AST_LEAF && node->token.type == T_STRING) {
    size_t len = node->token.value.string.length;
    if (len > (size_t)max_len) {
      out_err->type = Jsonv_MaxLength_error;
      out_err->path = path;
      snprintf(out_err->description, sizeof(out_err->description), "String too long, expected <= %d, got %lu.", max_len, len);
      return false;
    }
  }
  return true;
  /*#endregion*/
}

static bool handle_min_items(Jsonv_Context *ctx, ASTNode *pool, const Jsonv_Schema *schema, const uint8_t **pc, int node_idx, const char *path, E *out_err, const uint8_t *constant_pool) {
  /*#region*/
  (void)ctx; (void)schema; (void)constant_pool;
  int32_t min_items = read_int32(pc);
  ASTNode *node = &pool[node_idx];
  if (node->type == AST_ARRAY) {
    int count = 0;
    int child_idx = node->first_child;
    while (child_idx != -1) {
      count++;
      child_idx = pool[child_idx].next_sibling;
    }
    if (count < min_items) {
      out_err->type = Jsonv_MinItems_error;
      out_err->path = path;
      snprintf(out_err->description, sizeof(out_err->description), "Array has too few items, expected >= %d, got %d.", min_items, count);
      return false;
    }
  }
  return true;
  /*#endregion*/
}

static bool handle_max_items(Jsonv_Context *ctx, ASTNode *pool, const Jsonv_Schema *schema, const uint8_t **pc, int node_idx, const char *path, E *out_err, const uint8_t *constant_pool) {
  /*#region*/
  (void)ctx; (void)schema; (void)constant_pool;
  int32_t max_items = read_int32(pc);
  ASTNode *node = &pool[node_idx];
  if (node->type == AST_ARRAY) {
    int count = 0;
    int child_idx = node->first_child;
    while (child_idx != -1) {
      count++;
      child_idx = pool[child_idx].next_sibling;
    }
    if (count > max_items) {
      out_err->type = Jsonv_MinItems_error;
      out_err->path = path;
      snprintf(out_err->description, sizeof(out_err->description), "Array has too many items, expected <= %d, got %d.", max_items, count);
      return false;
    }
  }
  return true;
  /*#endregion*/
}

static bool handle_items(Jsonv_Context *ctx, ASTNode *pool, const Jsonv_Schema *schema, const uint8_t **pc, int node_idx, const char *path, E *out_err, const uint8_t *constant_pool) {
  /*#region*/
  (void)constant_pool;
  uint32_t items_offset = read_uint32(pc);
  ASTNode *node = &pool[node_idx];
  if (node->type == AST_ARRAY) {
    int child_idx = node->first_child;
    for (int i = 0; child_idx != -1; i++) {
      char item_path[64];
      snprintf(item_path, sizeof(item_path), "%s[%d]", path, i);
      char *arena_path = (char *)jsonv_arena_alloc(jsonv_ctx_arena(ctx), strlen(item_path) + 1);
      if (arena_path) {
        strcpy(arena_path, item_path);
      }
      if (!validate_bytecode(ctx, pool, schema, items_offset, child_idx, arena_path ? arena_path : path, out_err)) {
        return false;
      }
      child_idx = pool[child_idx].next_sibling;
    }
  }
  return true;
  /*#endregion*/
}

static bool handle_required(Jsonv_Context *ctx, ASTNode *pool, const Jsonv_Schema *schema, const uint8_t **pc, int node_idx, const char *path, E *out_err, const uint8_t *constant_pool) {
  /*#region*/
  (void)schema;
  uint32_t count = read_uint32(pc);
  ASTNode *node = &pool[node_idx];
  if (node->type == AST_OBJECT) {
    for (uint32_t i = 0; i < count; i++) {
      uint32_t req_offset = read_uint32(pc);
      uint32_t req_len = read_uint32(pc);
      const char *req_start = (const char *)(constant_pool + req_offset);
      
      // Make a temporary null-terminated string to look up
      char *req_key = (char *)jsonv_arena_alloc(jsonv_ctx_arena(ctx), req_len + 1);
      if (!req_key) return false;
      memcpy(req_key, req_start, req_len);
      req_key[req_len] = '\0';
      
      int val_idx = find_property(pool, node_idx, req_key);
      if (val_idx == -1) {
        out_err->type = Jsonv_Required_error;
        out_err->path = path;
        snprintf(out_err->description, sizeof(out_err->description), "Missing required field '%.*s'.", (int)req_len, req_start);
        return false;
      }
    }
  } else {
    *pc += count * (sizeof(uint32_t) + sizeof(uint32_t));
  }
  return true;
  /*#endregion*/
}

static bool handle_properties(Jsonv_Context *ctx, ASTNode *pool, const Jsonv_Schema *schema, const uint8_t **pc, int node_idx, const char *path, E *out_err, const uint8_t *constant_pool) {
  /*#region*/
  uint32_t prop_count = read_uint32(pc);
  uint32_t pattern_prop_count = read_uint32(pc);
  int32_t additional_props_rule = read_int32(pc);

  DecodedPropertyRule *props = NULL;
  DecodedPropertyRule stack_props[64];
  if (prop_count <= 64) {
    props = stack_props;
  } else {
    props = (DecodedPropertyRule *)jsonv_arena_alloc(jsonv_ctx_arena(ctx), prop_count * sizeof(DecodedPropertyRule));
    if (!props) return false;
  }

  for (uint32_t k = 0; k < prop_count; k++) {
    uint32_t key_offset = read_uint32(pc);
    props[k].key_len = read_uint32(pc);
    props[k].key_start = (const char *)(constant_pool + key_offset);
    props[k].rule_offset = read_uint32(pc);
  }

  DecodedPropertyRule *pattern_props = NULL;
  DecodedPropertyRule stack_pattern_props[64];
  if (pattern_prop_count <= 64) {
    pattern_props = stack_pattern_props;
  } else {
    pattern_props = (DecodedPropertyRule *)jsonv_arena_alloc(jsonv_ctx_arena(ctx), pattern_prop_count * sizeof(DecodedPropertyRule));
    if (!pattern_props) return false;
  }

  for (uint32_t k = 0; k < pattern_prop_count; k++) {
    uint32_t key_offset = read_uint32(pc);
    pattern_props[k].key_len = read_uint32(pc);
    pattern_props[k].key_start = (const char *)(constant_pool + key_offset);
    pattern_props[k].rule_offset = read_uint32(pc);
  }

  ASTNode *node = &pool[node_idx];
  if (node->type == AST_OBJECT) {
    int curr = node->first_child;
    while (curr != -1) {
      if (pool[curr].type != AST_SKIPPED) {
        Token k = pool[curr].token;
        int val_idx = pool[curr].next_sibling;

        // Extract key string view
        const char *k_start = (const char *)k.value.string.start;
        size_t k_len = k.value.string.length;
        if (k_len >= 2 && k_start[0] == '"' && k_start[k_len - 1] == '"') {
          k_start++;
          k_len -= 2;
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

        bool matched = false;

        // Match against properties
        for (uint32_t p = 0; p < prop_count; p++) {
          if (k_len == props[p].key_len && memcmp(k_start, props[p].key_start, k_len) == 0) {
            matched = true;
            if (val_idx != -1) {
              if (!validate_bytecode(ctx, pool, schema, props[p].rule_offset, val_idx, new_path ? new_path : path, out_err)) {
                return false;
              }
            }
            break;
          }
        }

        // Match against patternProperties
        bool compile_failed = false;
        for (uint32_t p = 0; p < pattern_prop_count; p++) {
          E temp_err = {0};
          if (regex_matches_key(k_start, k_len, pattern_props[p].key_start, pattern_props[p].key_len, ctx, path, &temp_err)) {
            matched = true;
            if (val_idx != -1) {
              if (!validate_bytecode(ctx, pool, schema, pattern_props[p].rule_offset, val_idx, new_path ? new_path : path, out_err)) {
                return false;
              }
            }
          } else if (temp_err.type == Jsonv_Compile_Regexp_Failed) {
            *out_err = temp_err;
            compile_failed = true;
            break;
          }
        }
        if (compile_failed) return false;

        // Match against additionalProperties if not matched by either
        if (!matched) {
          if (additional_props_rule == -2) {
            out_err->type = Jsonv_AdditionalProperties_error;
            out_err->path = new_path ? new_path : path;
            snprintf(out_err->description, sizeof(out_err->description), "Additional property not allowed.");
            return false;
          } else if (additional_props_rule >= 0) {
            if (val_idx != -1) {
              if (!validate_bytecode(ctx, pool, schema, (uint32_t)additional_props_rule, val_idx, new_path ? new_path : path, out_err)) {
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

static bool handle_multiple_of(Jsonv_Context *ctx, ASTNode *pool, const Jsonv_Schema *schema, const uint8_t **pc, int node_idx, const char *path, E *out_err, const uint8_t *constant_pool) {
  /*#region*/
  (void)ctx; (void)schema; (void)constant_pool;
  double mult_val = read_double(pc);
  ASTNode *node = &pool[node_idx];
  if (node->type == AST_LEAF && node->token.type == T_NUMBER) {
    double num = node->token.value.number;
    double quot = num / mult_val;
    double diff = quot - (double)(int64_t)(quot + (quot > 0.0 ? 0.5 : -0.5));
    if (diff < 0.0) diff = -diff;
    if (diff > 1e-9) {
      out_err->type = Jsonv_MultipleOf_error;
      out_err->path = path;
      snprintf(out_err->description, sizeof(out_err->description), "Value %g is not a multiple of %g.", num, mult_val);
      return false;
    }
  }
  return true;
  /*#endregion*/
}

static bool handle_exclusive_minimum(Jsonv_Context *ctx, ASTNode *pool, const Jsonv_Schema *schema, const uint8_t **pc, int node_idx, const char *path, E *out_err, const uint8_t *constant_pool) {
  /*#region*/
  (void)ctx; (void)schema; (void)constant_pool;
  double min_val = read_double(pc);
  ASTNode *node = &pool[node_idx];
  if (node->type == AST_LEAF && node->token.type == T_NUMBER) {
    double num = node->token.value.number;
    if (num <= min_val) {
      out_err->type = Jsonv_ExclusiveMinimum_error;
      out_err->path = path;
      snprintf(out_err->description, sizeof(out_err->description), "Value too small, expected > %.2f, got %.2f.", min_val, num);
      return false;
    }
  }
  return true;
  /*#endregion*/
}

static bool handle_exclusive_maximum(Jsonv_Context *ctx, ASTNode *pool, const Jsonv_Schema *schema, const uint8_t **pc, int node_idx, const char *path, E *out_err, const uint8_t *constant_pool) {
  /*#region*/
  (void)ctx; (void)schema; (void)constant_pool;
  double max_val = read_double(pc);
  ASTNode *node = &pool[node_idx];
  if (node->type == AST_LEAF && node->token.type == T_NUMBER) {
    double num = node->token.value.number;
    if (num >= max_val) {
      out_err->type = Jsonv_ExclusiveMaximum_error;
      out_err->path = path;
      snprintf(out_err->description, sizeof(out_err->description), "Value too large, expected < %.2f, got %.2f.", max_val, num);
      return false;
    }
  }
  return true;
  /*#endregion*/
}

static bool handle_pattern(Jsonv_Context *ctx, ASTNode *pool, const Jsonv_Schema *schema, const uint8_t **pc, int node_idx, const char *path, E *out_err, const uint8_t *constant_pool) {
  /*#region*/
  (void)schema;
  uint32_t pat_offset = read_uint32(pc);
  uint32_t pat_len = read_uint32(pc);
  const char *pat_start = (const char *)(constant_pool + pat_offset);
  ASTNode *node = &pool[node_idx];

  if (node->type == AST_LEAF && node->token.type == T_STRING) {
    const char *val_start = (const char *)node->token.value.string.start;
    size_t val_len = node->token.value.string.length;
    if (val_len >= 2 && val_start[0] == '"' && val_start[val_len - 1] == '"') {
      val_start++;
      val_len -= 2;
    }

    char *target_str = (char *)jsonv_arena_alloc(jsonv_ctx_arena(ctx), val_len + 1);
    if (!target_str) return false;
    memcpy(target_str, val_start, val_len);
    target_str[val_len] = '\0';

    char *pattern_str = (char *)jsonv_arena_alloc(jsonv_ctx_arena(ctx), pat_len + 1);
    if (!pattern_str) return false;
    memcpy(pattern_str, pat_start, pat_len);
    pattern_str[pat_len] = '\0';

    regex_t regex;
    if (regcomp(&regex, pattern_str, REG_EXTENDED | REG_NOSUB) != 0) {
      out_err->type = Jsonv_Compile_Regexp_Failed;
      out_err->path = path;
      snprintf(out_err->description, sizeof(out_err->description), "Failed to compile regex pattern '%s'.", pattern_str);
      return false;
    }

    int match_res = regexec(&regex, target_str, 0, NULL, 0);
    regfree(&regex);

    if (match_res != 0) {
      out_err->type = Jsonv_Pattern_error;
      out_err->path = path;
      snprintf(out_err->description, sizeof(out_err->description), "String '%s' does not match pattern '%s'.", target_str, pattern_str);
      return false;
    }
  }
  return true;
  /*#endregion*/
}

static bool handle_min_properties(Jsonv_Context *ctx, ASTNode *pool, const Jsonv_Schema *schema, const uint8_t **pc, int node_idx, const char *path, E *out_err, const uint8_t *constant_pool) {
  /*#region*/
  (void)ctx; (void)schema; (void)constant_pool;
  int32_t min_props = read_int32(pc);
  ASTNode *node = &pool[node_idx];
  if (node->type == AST_OBJECT) {
    int count = 0;
    int curr = node->first_child;
    while (curr != -1) {
      if (pool[curr].type != AST_SKIPPED) {
        count++;
      }
      int val_idx = pool[curr].next_sibling;
      if (val_idx == -1) break;
      curr = pool[val_idx].next_sibling;
    }
    if (count < min_props) {
      out_err->type = Jsonv_MinProperties_error;
      out_err->path = path;
      snprintf(out_err->description, sizeof(out_err->description), "Object has too few properties, expected >= %d, got %d.", min_props, count);
      return false;
    }
  }
  return true;
  /*#endregion*/
}

static bool handle_max_properties(Jsonv_Context *ctx, ASTNode *pool, const Jsonv_Schema *schema, const uint8_t **pc, int node_idx, const char *path, E *out_err, const uint8_t *constant_pool) {
  /*#region*/
  (void)ctx; (void)schema; (void)constant_pool;
  int32_t max_props = read_int32(pc);
  ASTNode *node = &pool[node_idx];
  if (node->type == AST_OBJECT) {
    int count = 0;
    int curr = node->first_child;
    while (curr != -1) {
      if (pool[curr].type != AST_SKIPPED) {
        count++;
      }
      int val_idx = pool[curr].next_sibling;
      if (val_idx == -1) break;
      curr = pool[val_idx].next_sibling;
    }
    if (count > max_props) {
      out_err->type = Jsonv_MaxProperties_error;
      out_err->path = path;
      snprintf(out_err->description, sizeof(out_err->description), "Object has too many properties, expected <= %d, got %d.", max_props, count);
      return false;
    }
  }
  return true;
  /*#endregion*/
}

static bool handle_unique_items(Jsonv_Context *ctx, ASTNode *pool, const Jsonv_Schema *schema, const uint8_t **pc, int node_idx, const char *path, E *out_err, const uint8_t *constant_pool) {
  /*#region*/
  (void)ctx; (void)schema; (void)pc; (void)constant_pool;
  ASTNode *node = &pool[node_idx];
  if (node->type == AST_ARRAY) {
    int c1 = node->first_child;
    while (c1 != -1) {
      if (pool[c1].type != AST_SKIPPED) {
        int c2 = pool[c1].next_sibling;
        while (c2 != -1) {
          if (pool[c2].type != AST_SKIPPED) {
            if (ast_nodes_equal(pool, c1, c2)) {
              out_err->type = Jsonv_UniqueItems_error;
              out_err->path = path;
              snprintf(out_err->description, sizeof(out_err->description), "Array items must be unique.");
              return false;
            }
          }
          c2 = pool[c2].next_sibling;
        }
      }
      c1 = pool[c1].next_sibling;
    }
  }
  return true;
  /*#endregion*/
}

static bool handle_contains(Jsonv_Context *ctx, ASTNode *pool, const Jsonv_Schema *schema, const uint8_t **pc, int node_idx, const char *path, E *out_err, const uint8_t *constant_pool) {
  /*#region*/
  (void)constant_pool;
  uint32_t contains_offset = read_uint32(pc);
  ASTNode *node = &pool[node_idx];
  if (node->type == AST_ARRAY) {
    bool contains_valid = false;
    int child_idx = node->first_child;
    while (child_idx != -1) {
      if (pool[child_idx].type != AST_SKIPPED) {
        E temp_err = {0};
        if (validate_bytecode(ctx, pool, schema, contains_offset, child_idx, path, &temp_err)) {
          contains_valid = true;
          break;
        }
      }
      child_idx = pool[child_idx].next_sibling;
    }
    if (!contains_valid) {
      out_err->type = Jsonv_Contains_error;
      out_err->path = path;
      snprintf(out_err->description, sizeof(out_err->description), "Array does not contain any item matching the subschema.");
      return false;
    }
  }
  return true;
  /*#endregion*/
}

static bool handle_not(Jsonv_Context *ctx, ASTNode *pool, const Jsonv_Schema *schema, const uint8_t **pc, int node_idx, const char *path, E *out_err, const uint8_t *constant_pool) {
  /*#region*/
  (void)constant_pool;
  uint32_t not_offset = read_uint32(pc);
  E temp_err = {0};
  if (validate_bytecode(ctx, pool, schema, not_offset, node_idx, path, &temp_err)) {
    out_err->type = Jsonv_Not_error;
    out_err->path = path;
    snprintf(out_err->description, sizeof(out_err->description), "Value must not validate against subschema.");
    return false;
  }
  return true;
  /*#endregion*/
}

static bool handle_all_of(Jsonv_Context *ctx, ASTNode *pool, const Jsonv_Schema *schema, const uint8_t **pc, int node_idx, const char *path, E *out_err, const uint8_t *constant_pool) {
  /*#region*/
  (void)constant_pool;
  uint32_t count = read_uint32(pc);
  bool all_valid = true;
  for (uint32_t i = 0; i < count; i++) {
    uint32_t sub_offset = read_uint32(pc);
    if (all_valid) {
      E temp_err = {0};
      if (!validate_bytecode(ctx, pool, schema, sub_offset, node_idx, path, &temp_err)) {
        all_valid = false;
      }
    }
  }
  if (!all_valid) {
    out_err->type = Jsonv_AllOf_error;
    out_err->path = path;
    snprintf(out_err->description, sizeof(out_err->description), "Value must validate against all subschemas in allOf.");
    return false;
  }
  return true;
  /*#endregion*/
}

static bool handle_any_of(Jsonv_Context *ctx, ASTNode *pool, const Jsonv_Schema *schema, const uint8_t **pc, int node_idx, const char *path, E *out_err, const uint8_t *constant_pool) {
  /*#region*/
  (void)constant_pool;
  uint32_t count = read_uint32(pc);
  bool any_valid = false;
  for (uint32_t i = 0; i < count; i++) {
    uint32_t sub_offset = read_uint32(pc);
    if (!any_valid) {
      E temp_err = {0};
      if (validate_bytecode(ctx, pool, schema, sub_offset, node_idx, path, &temp_err)) {
        any_valid = true;
      }
    }
  }
  if (!any_valid) {
    out_err->type = Jsonv_AnyOf_error;
    out_err->path = path;
    snprintf(out_err->description, sizeof(out_err->description), "Value must validate against at least one subschema in anyOf.");
    return false;
  }
  return true;
  /*#endregion*/
}

static bool handle_one_of(Jsonv_Context *ctx, ASTNode *pool, const Jsonv_Schema *schema, const uint8_t **pc, int node_idx, const char *path, E *out_err, const uint8_t *constant_pool) {
  /*#region*/
  (void)constant_pool;
  uint32_t count = read_uint32(pc);
  uint32_t valid_count = 0;
  for (uint32_t i = 0; i < count; i++) {
    uint32_t sub_offset = read_uint32(pc);
    E temp_err = {0};
    if (validate_bytecode(ctx, pool, schema, sub_offset, node_idx, path, &temp_err)) {
      valid_count++;
    }
  }
  if (valid_count != 1) {
    out_err->type = Jsonv_OneOf_error;
    out_err->path = path;
    snprintf(out_err->description, sizeof(out_err->description), "Value must validate against exactly one subschema in oneOf (validated against %u).", valid_count);
    return false;
  }
  return true;
  /*#endregion*/
}

static bool handle_if_then_else(Jsonv_Context *ctx, ASTNode *pool, const Jsonv_Schema *schema, const uint8_t **pc, int node_idx, const char *path, E *out_err, const uint8_t *constant_pool) {
  /*#region*/
  (void)constant_pool;
  uint32_t if_offset = read_uint32(pc);
  uint32_t then_offset = read_uint32(pc);
  uint32_t else_offset = read_uint32(pc);

  E temp_err = {0};
  bool if_passed = validate_bytecode(ctx, pool, schema, if_offset, node_idx, path, &temp_err);
  if (if_passed) {
    if (then_offset != (uint32_t)-1) {
      if (!validate_bytecode(ctx, pool, schema, then_offset, node_idx, path, out_err)) {
        return false;
      }
    }
  } else {
    if (else_offset != (uint32_t)-1) {
      if (!validate_bytecode(ctx, pool, schema, else_offset, node_idx, path, out_err)) {
        return false;
      }
    }
  }
  return true;
  /*#endregion*/
}

static bool handle_property_names(Jsonv_Context *ctx, ASTNode *pool, const Jsonv_Schema *schema, const uint8_t **pc, int node_idx, const char *path, E *out_err, const uint8_t *constant_pool) {
  /*#region*/
  (void)constant_pool;
  uint32_t sub_offset = read_uint32(pc);
  ASTNode *node = &pool[node_idx];
  if (node->type == AST_OBJECT) {
    int curr = node->first_child;
    while (curr != -1) {
      if (pool[curr].type != AST_SKIPPED) {
        Token k = pool[curr].token;
        const char *k_start = (const char *)k.value.string.start;
        size_t k_len = k.value.string.length;
        if (k_len >= 2 && k_start[0] == '"' && k_start[k_len - 1] == '"') {
          k_start++;
          k_len -= 2;
        }

        // Construct new path
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

        if (!validate_bytecode(ctx, pool, schema, sub_offset, curr, new_path ? new_path : path, out_err)) {
          return false;
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

static bool handle_format(Jsonv_Context *ctx, ASTNode *pool, const Jsonv_Schema *schema, const uint8_t **pc, int node_idx, const char *path, E *out_err, const uint8_t *constant_pool) {
  /*#region*/
  (void)ctx; (void)schema;
  uint32_t fmt_offset = read_uint32(pc);
  uint32_t fmt_len = read_uint32(pc);
  const char *fmt_start = (const char *)(constant_pool + fmt_offset);
  ASTNode *node = &pool[node_idx];

  if (node->type == AST_LEAF && node->token.type == T_STRING) {
    const char *val_start = (const char *)node->token.value.string.start;
    size_t val_len = node->token.value.string.length;
    if (val_len >= 2 && val_start[0] == '"' && val_start[val_len - 1] == '"') {
      val_start++;
      val_len -= 2;
    }

    bool valid = true;
    if (fmt_len == 4 && memcmp(fmt_start, "ipv4", 4) == 0) {
      valid = validate_ipv4(val_start, val_len);
    } else if (fmt_len == 5 && memcmp(fmt_start, "email", 5) == 0) {
      valid = validate_email(val_start, val_len);
    } else if (fmt_len == 4 && memcmp(fmt_start, "uuid", 4) == 0) {
      valid = validate_uuid(val_start, val_len);
    } else if (fmt_len == 9 && memcmp(fmt_start, "date-time", 9) == 0) {
      valid = validate_datetime(val_start, val_len);
    }

    if (!valid) {
      out_err->type = Jsonv_Format_error;
      out_err->path = path;
      snprintf(out_err->description, sizeof(out_err->description), "String '%.*s' does not conform to format '%.*s'.", (int)val_len, val_start, (int)fmt_len, fmt_start);
      return false;
    }
  }
  return true;
  /*#endregion*/
}

const OpcodeHandler opcode_handlers[] = {
  [OP_FAIL]              = handle_fail,
  [OP_TYPE]              = handle_type,
  [OP_MINIMUM]           = handle_minimum,
  [OP_MAXIMUM]           = handle_maximum,
  [OP_MIN_LENGTH]        = handle_min_length,
  [OP_MAX_LENGTH]        = handle_max_length,
  [OP_MIN_ITEMS]         = handle_min_items,
  [OP_MAX_ITEMS]         = handle_max_items,
  [OP_ITEMS]             = handle_items,
  [OP_REQUIRED]          = handle_required,
  [OP_PROPERTIES]        = handle_properties,
  [OP_MULTIPLE_OF]       = handle_multiple_of,
  [OP_EXCLUSIVE_MINIMUM] = handle_exclusive_minimum,
  [OP_EXCLUSIVE_MAXIMUM] = handle_exclusive_maximum,
  [OP_PATTERN]           = handle_pattern,
  [OP_MIN_PROPERTIES]    = handle_min_properties,
  [OP_MAX_PROPERTIES]    = handle_max_properties,
  [OP_UNIQUE_ITEMS]      = handle_unique_items,
  [OP_CONTAINS]          = handle_contains,
  [OP_NOT]               = handle_not,
  [OP_ALL_OF]            = handle_all_of,
  [OP_ANY_OF]            = handle_any_of,
  [OP_ONE_OF]            = handle_one_of,
  [OP_IF_THEN_ELSE]      = handle_if_then_else,
  [OP_PROPERTY_NAMES]    = handle_property_names,
  [OP_FORMAT]            = handle_format,
};
