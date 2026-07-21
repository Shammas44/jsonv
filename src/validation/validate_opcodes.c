#include "validate_internal.h"
#include "arena.internal.h"
#include "parser.h"
#include "unescape.h"

static double token_get_double(Token t) {
  /*#region*/
  if (t.type == T_NUMBER) {
    char buf[128];
    size_t len = t.value.raw_number.length < sizeof(buf) - 1 ? t.value.raw_number.length : sizeof(buf) - 1;
    memcpy(buf, t.value.raw_number.start, len);
    buf[len] = '\0';
    return strtod(buf, NULL);
  }
  return 0.0;
  /*#endregion*/
}

static bool handle_fail(VMState *state) {
  /*#region*/
  state->out_err->type = Jsonv_ValueNotAllowed_error;
  state->out_err->path = state->path;
  snprintf(state->out_err->description, sizeof(state->out_err->description), "Value not allowed (Schema is false).");
  return false;
  /*#endregion*/
}

static bool handle_type(VMState *state) {
  /*#region*/
  uint32_t type_mask = read_uint32(&state->pc);
  ASTNode *node = &state->pool[state->node_idx];
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
      case T_NUMBER: {
        double d = token_get_double(node->token);
        match = (type_mask & TYPE_NUMBER) || 
                ((type_mask & TYPE_INTEGER) && (d == (int64_t)d));
        break;
      }
      case T_STRING:
        match = (type_mask & TYPE_STRING);
        break;
      default:
        break;
    }
  }
  
  if (!match) {
    state->out_err->type = Jsonv_Type_error;
    state->out_err->path = state->path;
    snprintf(state->out_err->description, sizeof(state->out_err->description), "Type mismatch.");
    return false;
  }
  return true;
  /*#endregion*/
}

static bool handle_minimum(VMState *state) {
  /*#region*/
  double min_val = read_double(&state->pc);
  ASTNode *node = &state->pool[state->node_idx];
  if (node->type == AST_LEAF && node->token.type == T_NUMBER) {
    double num = token_get_double(node->token);
    if (num < min_val) {
      state->out_err->type = Jsonv_Minimum_error;
      state->out_err->path = state->path;
      snprintf(state->out_err->description, sizeof(state->out_err->description), "Value too small, expected >= %.2f, got %.2f.", min_val, num);
      return false;
    }
  }
  return true;
  /*#endregion*/
}

static bool handle_maximum(VMState *state) {
  /*#region*/
  double max_val = read_double(&state->pc);
  ASTNode *node = &state->pool[state->node_idx];
  if (node->type == AST_LEAF && node->token.type == T_NUMBER) {
    double num = token_get_double(node->token);
    if (num > max_val) {
      state->out_err->type = Jsonv_Maximum_error;
      state->out_err->path = state->path;
      snprintf(state->out_err->description, sizeof(state->out_err->description), "Value too large, expected <= %.2f, got %.2f.", max_val, num);
      return false;
    }
  }
  return true;
  /*#endregion*/
}

static bool handle_min_length(VMState *state) {
  /*#region*/
  int32_t min_len = read_int32(&state->pc);
  ASTNode *node = &state->pool[state->node_idx];
  if (node->type == AST_LEAF && node->token.type == T_STRING) {
    size_t len = node->token.value.string.length;
    if (len < (size_t)min_len) {
      state->out_err->type = Jsonv_MinLength_error;
      state->out_err->path = state->path;
      snprintf(state->out_err->description, sizeof(state->out_err->description), "String too short, expected >= %d, got %lu.", min_len, len);
      return false;
    }
  }
  return true;
  /*#endregion*/
}

static bool handle_max_length(VMState *state) {
  /*#region*/
  int32_t max_len = read_int32(&state->pc);
  ASTNode *node = &state->pool[state->node_idx];
  if (node->type == AST_LEAF && node->token.type == T_STRING) {
    size_t len = node->token.value.string.length;
    if (len > (size_t)max_len) {
      state->out_err->type = Jsonv_MaxLength_error;
      state->out_err->path = state->path;
      snprintf(state->out_err->description, sizeof(state->out_err->description), "String too long, expected <= %d, got %lu.", max_len, len);
      return false;
    }
  }
  return true;
  /*#endregion*/
}

static bool handle_min_items(VMState *state) {
  /*#region*/
  int32_t min_items = read_int32(&state->pc);
  ASTNode *node = &state->pool[state->node_idx];
  if (node->type == AST_ARRAY) {
    int count = 0;
    int child_idx = node->first_child;
    while (child_idx != -1) {
      count++;
      child_idx = state->pool[child_idx].next_sibling;
    }
    if (count < min_items) {
      state->out_err->type = Jsonv_MinItems_error;
      state->out_err->path = state->path;
      snprintf(state->out_err->description, sizeof(state->out_err->description), "Array has too few items, expected >= %d, got %d.", min_items, count);
      return false;
    }
  }
  return true;
  /*#endregion*/
}

static bool handle_max_items(VMState *state) {
  /*#region*/
  int32_t max_items = read_int32(&state->pc);
  ASTNode *node = &state->pool[state->node_idx];
  if (node->type == AST_ARRAY) {
    int count = 0;
    int child_idx = node->first_child;
    while (child_idx != -1) {
      count++;
      child_idx = state->pool[child_idx].next_sibling;
    }
    if (count > max_items) {
      state->out_err->type = Jsonv_MinItems_error;
      state->out_err->path = state->path;
      snprintf(state->out_err->description, sizeof(state->out_err->description), "Array has too many items, expected <= %d, got %d.", max_items, count);
      return false;
    }
  }
  return true;
  /*#endregion*/
}

static bool handle_items(VMState *state) {
  /*#region*/
  uint32_t items_offset = read_uint32(&state->pc);
  ASTNode *node = &state->pool[state->node_idx];
  if (node->type == AST_ARRAY) {
    int child_idx = node->first_child;
    for (int i = 0; child_idx != -1; i++) {
      char item_path[64];
      snprintf(item_path, sizeof(item_path), "%s[%d]", state->path, i);

      // No need to handle null pointer, error is automatically raised
      char *arena_path = (char *)arena_alloc(jsonv_ctx_arena(state->ctx), strlen(item_path) + 1);
      if (arena_path) {
        strcpy(arena_path, item_path);
      }
      if (!validate_bytecode(state->ctx, state->pool, state->schema, items_offset, child_idx, arena_path ? arena_path : state->path, state->out_err)) {
        return false;
      }
      child_idx = state->pool[child_idx].next_sibling;
    }
  }
  return true;
  /*#endregion*/
}

static bool handle_required(VMState *state) {
  /*#region*/
  uint32_t count = read_uint32(&state->pc);
  ASTNode *node = &state->pool[state->node_idx];
  if (node->type == AST_OBJECT) {
    for (uint32_t i = 0; i < count; i++) {
      uint32_t req_offset = read_uint32(&state->pc);
      uint32_t req_len = read_uint32(&state->pc);
      const char *req_start = (const char *)(state->constant_pool + req_offset);
      
      // Make a temporary null-terminated string to look up
      char *req_key = (char *)arena_alloc(jsonv_ctx_arena(state->ctx), req_len + 1);
      if (!req_key) return false;
      memcpy(req_key, req_start, req_len);
      req_key[req_len] = '\0';
      
      int val_idx = find_property(state->pool, state->node_idx, req_key);
      if (val_idx == -1) {
        state->out_err->type = Jsonv_Required_error;
        state->out_err->path = state->path;
        snprintf(state->out_err->description, sizeof(state->out_err->description), "Missing required field '%.*s'.", (int)req_len, req_start);
        return false;
      }
    }
  } else {
    state->pc += count * (sizeof(uint32_t) + sizeof(uint32_t));
  }
  return true;
  /*#endregion*/
}

static bool handle_properties(VMState *state) {
  /*#region*/
  uint32_t prop_count = read_uint32(&state->pc);
  uint32_t pattern_prop_count = read_uint32(&state->pc);
  int32_t additional_props_rule = read_int32(&state->pc);

  DecodedPropertyRule *props = NULL;
  DecodedPropertyRule stack_props[64];
  if (prop_count <= 64) {
    props = stack_props;
  } else {
    props = (DecodedPropertyRule *)arena_alloc(jsonv_ctx_arena(state->ctx), prop_count * sizeof(DecodedPropertyRule));
    if (!props) return false;
  }

  for (uint32_t k = 0; k < prop_count; k++) {
    uint32_t key_offset = read_uint32(&state->pc);
    props[k].key_len = read_uint32(&state->pc);
    props[k].key_start = (const char *)(state->constant_pool + key_offset);
    props[k].rule_offset = read_uint32(&state->pc);
  }

  DecodedPropertyRule *pattern_props = NULL;
  DecodedPropertyRule stack_pattern_props[64];
  if (pattern_prop_count <= 64) {
    pattern_props = stack_pattern_props;
  } else {
    pattern_props = (DecodedPropertyRule *)arena_alloc(jsonv_ctx_arena(state->ctx), pattern_prop_count * sizeof(DecodedPropertyRule));
    if (!pattern_props) return false;
  }

  for (uint32_t k = 0; k < pattern_prop_count; k++) {
    uint32_t key_offset = read_uint32(&state->pc);
    pattern_props[k].key_len = read_uint32(&state->pc);
    pattern_props[k].key_start = (const char *)(state->constant_pool + key_offset);
    pattern_props[k].rule_offset = read_uint32(&state->pc);
  }

  ASTNode *node = &state->pool[state->node_idx];
  if (node->type == AST_OBJECT) {
    int curr = node->first_child;
    while (curr != -1) {
      if (state->pool[curr].type != AST_SKIPPED) {
        Token k = state->pool[curr].token;
        int val_idx = state->pool[curr].next_sibling;

        // Extract key string view
        const char *k_start = (const char *)k.value.string.start;
        size_t k_len = k.value.string.length;
        if (k_len >= 2 && k_start[0] == '"' && k_start[k_len - 1] == '"') {
          k_start++;
          k_len -= 2;
        }

        if (k.has_escape) {
          char stack_buf[256];
          char *ubuf = (k_len + 1 <= sizeof(stack_buf)) ? stack_buf : (char *)arena_alloc(jsonv_ctx_arena(state->ctx), k_len + 1);
          if (ubuf) {
            k_len = jsonv_unescape_string((const unsigned char *)k_start, k_len, ubuf);
            k_start = ubuf;
          }
        }

        // Construct new path for validation errors
        size_t p_len = strlen(state->path);
        size_t needed = p_len + k_len + 2;
        char *new_path = (char *)arena_alloc(jsonv_ctx_arena(state->ctx), needed);
        if (new_path) {
          if (p_len > 0) {
            snprintf(new_path, needed, "%s.%.*s", state->path, (int)k_len, k_start);
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
              if (!validate_bytecode(state->ctx, state->pool, state->schema, props[p].rule_offset, val_idx, new_path ? new_path : state->path, state->out_err)) {
                return false;
              }
            }
            break;
          }
        }

        // Match against patternProperties
        bool compile_failed = false;
        for (uint32_t p = 0; p < pattern_prop_count; p++) {
          Jsonv_Error temp_err = {0};
          if (regex_matches_key(k_start, k_len, pattern_props[p].key_start, pattern_props[p].key_len, state->ctx, state->path, &temp_err)) {
            matched = true;
            if (val_idx != -1) {
              if (!validate_bytecode(state->ctx, state->pool, state->schema, pattern_props[p].rule_offset, val_idx, new_path ? new_path : state->path, state->out_err)) {
                return false;
              }
            }
          } else if (temp_err.type == Jsonv_Compile_Regexp_Failed) {
            *state->out_err = temp_err;
            compile_failed = true;
            break;
          }
        }
        if (compile_failed) return false;

        // Match against additionalProperties if not matched by either
        if (!matched) {
          if (additional_props_rule == -2) {
            state->out_err->type = Jsonv_AdditionalProperties_error;
            state->out_err->path = new_path ? new_path : state->path;
            snprintf(state->out_err->description, sizeof(state->out_err->description), "Additional property not allowed.");
            return false;
          } else if (additional_props_rule >= 0) {
            if (val_idx != -1) {
              if (!validate_bytecode(state->ctx, state->pool, state->schema, (uint32_t)additional_props_rule, val_idx, new_path ? new_path : state->path, state->out_err)) {
                return false;
              }
            }
          }
        }
      }

      int val_idx = state->pool[curr].next_sibling;
      if (val_idx == -1) break;
      curr = state->pool[val_idx].next_sibling;
    }
  }
  return true;
  /*#endregion*/
}

static bool handle_multiple_of(VMState *state) {
  /*#region*/
  double mult_val = read_double(&state->pc);
  ASTNode *node = &state->pool[state->node_idx];
  if (node->type == AST_LEAF && node->token.type == T_NUMBER) {
    double num = token_get_double(node->token);
    double quot = num / mult_val;
    double diff = quot - (double)(int64_t)(quot + (quot > 0.0 ? 0.5 : -0.5));
    if (diff < 0.0) diff = -diff;
    if (diff > 1e-9) {
      state->out_err->type = Jsonv_MultipleOf_error;
      state->out_err->path = state->path;
      snprintf(state->out_err->description, sizeof(state->out_err->description), "Value %g is not a multiple of %g.", num, mult_val);
      return false;
    }
  }
  return true;
  /*#endregion*/
}

static bool handle_exclusive_minimum(VMState *state) {
  /*#region*/
  double min_val = read_double(&state->pc);
  ASTNode *node = &state->pool[state->node_idx];
  if (node->type == AST_LEAF && node->token.type == T_NUMBER) {
    double num = token_get_double(node->token);
    if (num <= min_val) {
      state->out_err->type = Jsonv_ExclusiveMinimum_error;
      state->out_err->path = state->path;
      snprintf(state->out_err->description, sizeof(state->out_err->description), "Value too small, expected > %.2f, got %.2f.", min_val, num);
      return false;
    }
  }
  return true;
  /*#endregion*/
}

static bool handle_exclusive_maximum(VMState *state) {
  /*#region*/
  double max_val = read_double(&state->pc);
  ASTNode *node = &state->pool[state->node_idx];
  if (node->type == AST_LEAF && node->token.type == T_NUMBER) {
    double num = token_get_double(node->token);
    if (num >= max_val) {
      state->out_err->type = Jsonv_ExclusiveMaximum_error;
      state->out_err->path = state->path;
      snprintf(state->out_err->description, sizeof(state->out_err->description), "Value too large, expected < %.2f, got %.2f.", max_val, num);
      return false;
    }
  }
  return true;
  /*#endregion*/
}

static bool handle_pattern(VMState *state) {
  /*#region*/
  uint32_t pat_offset = read_uint32(&state->pc);
  uint32_t pat_len = read_uint32(&state->pc);
  const char *pat_start = (const char *)(state->constant_pool + pat_offset);
  ASTNode *node = &state->pool[state->node_idx];

  if (node->type == AST_LEAF && node->token.type == T_STRING) {
    const char *val_start = (const char *)node->token.value.string.start;
    size_t val_len = node->token.value.string.length;
    if (val_len >= 2 && val_start[0] == '"' && val_start[val_len - 1] == '"') {
      val_start++;
      val_len -= 2;
    }

    char *target_str = (char *)arena_alloc(jsonv_ctx_arena(state->ctx), val_len + 1);
    if (!target_str) return false;
    if (!node->token.has_escape) {
      memcpy(target_str, val_start, val_len);
      target_str[val_len] = '\0';
    } else {
      size_t ulen = jsonv_unescape_string((const unsigned char *)val_start, val_len, target_str);
      target_str[ulen] = '\0';
    }

    char *pattern_str = (char *)arena_alloc(jsonv_ctx_arena(state->ctx), pat_len + 1);
    if (!pattern_str) return false;
    memcpy(pattern_str, pat_start, pat_len);
    pattern_str[pat_len] = '\0';

    regex_t regex;
    if (regcomp(&regex, pattern_str, REG_EXTENDED | REG_NOSUB) != 0) {
      state->out_err->type = Jsonv_Compile_Regexp_Failed;
      state->out_err->path = state->path;
      snprintf(state->out_err->description, sizeof(state->out_err->description), "Failed to compile regex pattern '%s'.", pattern_str);
      return false;
    }

    int match_res = regexec(&regex, target_str, 0, NULL, 0);
    regfree(&regex);

    if (match_res != 0) {
      state->out_err->type = Jsonv_Pattern_error;
      state->out_err->path = state->path;
      snprintf(state->out_err->description, sizeof(state->out_err->description), "String '%s' does not match pattern '%s'.", target_str, pattern_str);
      return false;
    }
  }
  return true;
  /*#endregion*/
}

static bool handle_min_properties(VMState *state) {
  /*#region*/
  int32_t min_props = read_int32(&state->pc);
  ASTNode *node = &state->pool[state->node_idx];
  if (node->type == AST_OBJECT) {
    int count = 0;
    int curr = node->first_child;
    while (curr != -1) {
      if (state->pool[curr].type != AST_SKIPPED) {
        count++;
      }
      int val_idx = state->pool[curr].next_sibling;
      if (val_idx == -1) break;
      curr = state->pool[val_idx].next_sibling;
    }
    if (count < min_props) {
      state->out_err->type = Jsonv_MinProperties_error;
      state->out_err->path = state->path;
      snprintf(state->out_err->description, sizeof(state->out_err->description), "Object has too few properties, expected >= %d, got %d.", min_props, count);
      return false;
    }
  }
  return true;
  /*#endregion*/
}

static bool handle_max_properties(VMState *state) {
  /*#region*/
  int32_t max_props = read_int32(&state->pc);
  ASTNode *node = &state->pool[state->node_idx];
  if (node->type == AST_OBJECT) {
    int count = 0;
    int curr = node->first_child;
    while (curr != -1) {
      if (state->pool[curr].type != AST_SKIPPED) {
        count++;
      }
      int val_idx = state->pool[curr].next_sibling;
      if (val_idx == -1) break;
      curr = state->pool[val_idx].next_sibling;
    }
    if (count > max_props) {
      state->out_err->type = Jsonv_MaxProperties_error;
      state->out_err->path = state->path;
      snprintf(state->out_err->description, sizeof(state->out_err->description), "Object has too many properties, expected <= %d, got %d.", max_props, count);
      return false;
    }
  }
  return true;
  /*#endregion*/
}

static bool handle_unique_items(VMState *state) {
  /*#region*/
  ASTNode *node = &state->pool[state->node_idx];
  if (node->type == AST_ARRAY) {
    int c1 = node->first_child;
    while (c1 != -1) {
      if (state->pool[c1].type != AST_SKIPPED) {
        int c2 = state->pool[c1].next_sibling;
        while (c2 != -1) {
          if (state->pool[c2].type != AST_SKIPPED) {
            if (ast_nodes_equal(state->pool, c1, c2)) {
              state->out_err->type = Jsonv_UniqueItems_error;
              state->out_err->path = state->path;
              snprintf(state->out_err->description, sizeof(state->out_err->description), "Array items must be unique.");
              return false;
            }
          }
          c2 = state->pool[c2].next_sibling;
        }
      }
      c1 = state->pool[c1].next_sibling;
    }
  }
  return true;
  /*#endregion*/
}

static bool handle_contains(VMState *state) {
  /*#region*/
  uint32_t contains_offset = read_uint32(&state->pc);
  ASTNode *node = &state->pool[state->node_idx];
  if (node->type == AST_ARRAY) {
    bool contains_valid = false;
    int child_idx = node->first_child;
    while (child_idx != -1) {
      if (state->pool[child_idx].type != AST_SKIPPED) {
        Jsonv_Error temp_err = {0};
        if (validate_bytecode(state->ctx, state->pool, state->schema, contains_offset, child_idx, state->path, &temp_err)) {
          contains_valid = true;
          break;
        }
      }
      child_idx = state->pool[child_idx].next_sibling;
    }
    if (!contains_valid) {
      state->out_err->type = Jsonv_Contains_error;
      state->out_err->path = state->path;
      snprintf(state->out_err->description, sizeof(state->out_err->description), "Array does not contain any item matching the subschema.");
      return false;
    }
  }
  return true;
  /*#endregion*/
}

static bool handle_not(VMState *state) {
  /*#region*/
  uint32_t not_offset = read_uint32(&state->pc);
  Jsonv_Error temp_err = {0};
  if (validate_bytecode(state->ctx, state->pool, state->schema, not_offset, state->node_idx, state->path, &temp_err)) {
    state->out_err->type = Jsonv_Not_error;
    state->out_err->path = state->path;
    snprintf(state->out_err->description, sizeof(state->out_err->description), "Value must not validate against subschema.");
    return false;
  }
  return true;
  /*#endregion*/
}

static bool handle_all_of(VMState *state) {
  /*#region*/
  uint32_t count = read_uint32(&state->pc);
  bool all_valid = true;
  for (uint32_t i = 0; i < count; i++) {
    uint32_t sub_offset = read_uint32(&state->pc);
    if (all_valid) {
      Jsonv_Error temp_err = {0};
      if (!validate_bytecode(state->ctx, state->pool, state->schema, sub_offset, state->node_idx, state->path, &temp_err)) {
        all_valid = false;
      }
    }
  }
  if (!all_valid) {
    state->out_err->type = Jsonv_AllOf_error;
    state->out_err->path = state->path;
    snprintf(state->out_err->description, sizeof(state->out_err->description), "Value must validate against all subschemas in allOf.");
    return false;
  }
  return true;
  /*#endregion*/
}

static bool handle_any_of(VMState *state) {
  /*#region*/
  uint32_t count = read_uint32(&state->pc);
  bool any_valid = false;
  for (uint32_t i = 0; i < count; i++) {
    uint32_t sub_offset = read_uint32(&state->pc);
    if (!any_valid) {
      Jsonv_Error temp_err = {0};
      if (validate_bytecode(state->ctx, state->pool, state->schema, sub_offset, state->node_idx, state->path, &temp_err)) {
        any_valid = true;
      }
    }
  }
  if (!any_valid) {
    state->out_err->type = Jsonv_AnyOf_error;
    state->out_err->path = state->path;
    snprintf(state->out_err->description, sizeof(state->out_err->description), "Value must validate against at least one subschema in anyOf.");
    return false;
  }
  return true;
  /*#endregion*/
}

static bool handle_one_of(VMState *state) {
  /*#region*/
  uint32_t count = read_uint32(&state->pc);
  uint32_t valid_count = 0;
  for (uint32_t i = 0; i < count; i++) {
    uint32_t sub_offset = read_uint32(&state->pc);
    Jsonv_Error temp_err = {0};
    if (validate_bytecode(state->ctx, state->pool, state->schema, sub_offset, state->node_idx, state->path, &temp_err)) {
      valid_count++;
    }
  }
  if (valid_count != 1) {
    state->out_err->type = Jsonv_OneOf_error;
    state->out_err->path = state->path;
    snprintf(state->out_err->description, sizeof(state->out_err->description), "Value must validate against exactly one subschema in oneOf (validated against %u).", valid_count);
    return false;
  }
  return true;
  /*#endregion*/
}

static bool handle_if_then_else(VMState *state) {
  /*#region*/
  uint32_t if_offset = read_uint32(&state->pc);
  uint32_t then_offset = read_uint32(&state->pc);
  uint32_t else_offset = read_uint32(&state->pc);

  Jsonv_Error temp_err = {0};
  bool if_passed = validate_bytecode(state->ctx, state->pool, state->schema, if_offset, state->node_idx, state->path, &temp_err);
  if (if_passed) {
    if (then_offset != (uint32_t)-1) {
      if (!validate_bytecode(state->ctx, state->pool, state->schema, then_offset, state->node_idx, state->path, state->out_err)) {
        return false;
      }
    }
  } else {
    if (else_offset != (uint32_t)-1) {
      if (!validate_bytecode(state->ctx, state->pool, state->schema, else_offset, state->node_idx, state->path, state->out_err)) {
        return false;
      }
    }
  }
  return true;
  /*#endregion*/
}

static bool handle_property_names(VMState *state) {
  /*#region*/
  uint32_t sub_offset = read_uint32(&state->pc);
  ASTNode *node = &state->pool[state->node_idx];
  if (node->type == AST_OBJECT) {
    int curr = node->first_child;
    while (curr != -1) {
      if (state->pool[curr].type != AST_SKIPPED) {
        Token k = state->pool[curr].token;
        const char *k_start = (const char *)k.value.string.start;
        size_t k_len = k.value.string.length;
        if (k_len >= 2 && k_start[0] == '"' && k_start[k_len - 1] == '"') {
          k_start++;
          k_len -= 2;
        }

        // Construct new path
        size_t p_len = strlen(state->path);
        size_t needed = p_len + k_len + 2;
        char *new_path = (char *)arena_alloc(jsonv_ctx_arena(state->ctx), needed);
        if (new_path) {
          if (p_len > 0) {
            snprintf(new_path, needed, "%s.%.*s", state->path, (int)k_len, k_start);
          } else {
            snprintf(new_path, needed, "%.*s", (int)k_len, k_start);
          }
        }

        if (!validate_bytecode(state->ctx, state->pool, state->schema, sub_offset, curr, new_path ? new_path : state->path, state->out_err)) {
          return false;
        }
      }
      int val_idx = state->pool[curr].next_sibling;
      if (val_idx == -1) break;
      curr = state->pool[val_idx].next_sibling;
    }
  }
  return true;
  /*#endregion*/
}

static bool handle_format(VMState *state) {
  /*#region*/
  uint32_t fmt_offset = read_uint32(&state->pc);
  uint32_t fmt_len = read_uint32(&state->pc);
  const char *fmt_start = (const char *)(state->constant_pool + fmt_offset);
  ASTNode *node = &state->pool[state->node_idx];

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
      state->out_err->type = Jsonv_Format_error;
      state->out_err->path = state->path;
      snprintf(state->out_err->description, sizeof(state->out_err->description), "String '%.*s' does not conform to format '%.*s'.", (int)val_len, val_start, (int)fmt_len, fmt_start);
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
