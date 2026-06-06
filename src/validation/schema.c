#include "schema.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// --- HELPERS ---

static inline bool is_ast_string(const ASTNode *n) {
  /*#region*/
  return n->type == AST_STRING || (n->type == AST_LEAF && n->token.type == T_STRING);
  /*#endregion*/
}

static inline bool is_ast_true(const ASTNode *n) {
  /*#region*/
  return n->type == AST_TRUE || (n->type == AST_LEAF && n->token.type == T_TRUE);
  /*#endregion*/
}

static inline bool is_ast_false(const ASTNode *n) {
  /*#region*/
  return n->type == AST_FALSE || (n->type == AST_LEAF && n->token.type == T_FALSE);
  /*#endregion*/
}

bool token_equals(Token t, const char *str) {
  /*#region*/
  if (t.type != T_STRING)
    return false;

  const char *t_start = (const char *)t.value.string.start;
  size_t t_len = t.value.string.length;

  if (t_len >= 2 && t_start[0] == '"' && t_start[t_len - 1] == '"') {
    t_start++;
    t_len -= 2;
  }

  size_t str_len = strlen(str);
  if (t_len != str_len)
    return false;

  return strncmp(t_start, str, str_len) == 0;
  /*#endregion*/
}

double parse_number(Token t) {
  /*#region*/
  if (t.type == T_NUMBER) {
    return t.value.number;
  }
  return 0.0;
  /*#endregion*/
}

int map_type_string_to_mask(Token t) {
  /*#region*/
  if (token_equals(t, "string"))
    return TYPE_STRING;
  if (token_equals(t, "number"))
    return TYPE_NUMBER;
  if (token_equals(t, "integer"))
    return TYPE_INTEGER;
  if (token_equals(t, "boolean"))
    return TYPE_BOOL;
  if (token_equals(t, "object"))
    return TYPE_OBJECT;
  if (token_equals(t, "array"))
    return TYPE_ARRAY;
  if (token_equals(t, "null"))
    return TYPE_NULL;
  return 0;
  /*#endregion*/
}

static inline void emit_byte(uint8_t **pc, uint8_t val) {
  /*#region*/
  **pc = val;
  (*pc)++;
  /*#endregion*/
}

static inline void emit_uint32(uint8_t **pc, uint32_t val) {
  /*#region*/
  memcpy(*pc, &val, sizeof(val));
  *pc += sizeof(val);
  /*#endregion*/
}

static inline void emit_int32(uint8_t **pc, int32_t val) {
  /*#region*/
  memcpy(*pc, &val, sizeof(val));
  *pc += sizeof(val);
  /*#endregion*/
}

static inline void emit_double(uint8_t **pc, double val) {
  /*#region*/
  memcpy(*pc, &val, sizeof(val));
  *pc += sizeof(val);
  /*#endregion*/
}


static int object_find_key_val_idx(const ASTNode *nodes, int obj_idx, const char *key_name) {
  /*#region*/
  int key_idx = nodes[obj_idx].first_child;
  while (key_idx != -1) {
    if (token_equals(nodes[key_idx].token, key_name)) {
      return key_idx + 1;
    }
    key_idx = nodes[key_idx + 1].next_sibling;
  }
  return -1;
  /*#endregion*/
}

static const char *get_stripped_string(Token t, uint32_t *out_len) {
  /*#region*/
  if (t.type != T_STRING) {
    *out_len = 0;
    return NULL;
  }
  const char *start = (const char *)t.value.string.start;
  size_t len = t.value.string.length;
  if (len >= 2 && start[0] == '"' && start[len - 1] == '"') {
    start++;
    len -= 2;
  }
  *out_len = (uint32_t)len;
  return start;
  /*#endregion*/
}

static inline void emit_string_ref(uint8_t **pc, uint8_t *bytecode, uint32_t constant_pool_start, uint32_t *data_write_ptr, Token t) {
  /*#region*/
  uint32_t len = 0;
  const char *str = get_stripped_string(t, &len);
  uint32_t rel_offset = *data_write_ptr - constant_pool_start;
  emit_uint32(pc, rel_offset);
  emit_uint32(pc, len);
  if (len > 0 && str != NULL) {
    memcpy(bytecode + *data_write_ptr, str, len);
    *data_write_ptr += len;
  }
  /*#endregion*/
}

// --- DIRECT COMPILER PASS 1: SIZE CALCULATION ---

static uint32_t calculate_schema_size(ASTNode *nodes, int node_idx, uint32_t *offsets, uint32_t current_offset, uint32_t *data_size) {
  /*#region*/
  offsets[node_idx] = current_offset;
  uint32_t own_size = 0;

  if (is_ast_false(&nodes[node_idx])) {
    return 1; // OP_FAIL
  }
  if (is_ast_true(&nodes[node_idx]) || nodes[node_idx].type != AST_OBJECT) {
    return 1; // OP_END
  }

  // Iterate over properties of this schema object
  bool has_type = false;
  bool has_min = false;
  bool has_max = false;
  bool has_min_len = false;
  bool has_max_len = false;
  bool has_min_items = false;
  bool has_max_items = false;
  bool has_items = false;
  bool has_contains = false;
  bool has_not = false;
  bool has_all_of = false;
  int all_of_count = 0;
  bool has_any_of = false;
  int any_of_count = 0;
  bool has_one_of = false;
  int one_of_count = 0;
  int required_count = 0;
  int prop_count = 0;
  bool has_additional_props = false;
  bool has_multiple_of = false;
  bool has_ex_min = false;
  bool has_ex_max = false;
  bool has_pattern = false;
  bool has_min_props = false;
  bool has_max_props = false;
  bool has_unique_items = false;
  bool has_if = false;
  bool has_property_names = false;
  int pattern_prop_count = 0;
  bool has_format = false;

  int key_idx = nodes[node_idx].first_child;
  while (key_idx != -1) {
    ASTNode *key = &nodes[key_idx];
    ASTNode *val = &nodes[key_idx + 1];

    if (token_equals(key->token, "type")) {
      has_type = true;
    } else if (token_equals(key->token, "minimum")) {
      has_min = true;
    } else if (token_equals(key->token, "maximum")) {
      has_max = true;
    } else if (token_equals(key->token, "multipleOf")) {
      has_multiple_of = true;
    } else if (token_equals(key->token, "exclusiveMinimum")) {
      has_ex_min = true;
    } else if (token_equals(key->token, "exclusiveMaximum")) {
      has_ex_max = true;
    } else if (token_equals(key->token, "pattern")) {
      has_pattern = true;
      uint32_t len = 0;
      get_stripped_string(val->token, &len);
      *data_size += len;
    } else if (token_equals(key->token, "minProperties")) {
      has_min_props = true;
    } else if (token_equals(key->token, "maxProperties")) {
      has_max_props = true;
    } else if (token_equals(key->token, "uniqueItems")) {
      has_unique_items = is_ast_true(val);
    } else if (token_equals(key->token, "if")) {
      if (val->type == AST_OBJECT || is_ast_true(val) || is_ast_false(val)) {
        has_if = true;
      }
    } else if (token_equals(key->token, "propertyNames")) {
      if (val->type == AST_OBJECT || is_ast_true(val) || is_ast_false(val)) {
        has_property_names = true;
      }
    } else if (token_equals(key->token, "format")) {
      has_format = true;
      uint32_t len = 0;
      get_stripped_string(val->token, &len);
      *data_size += len;
    } else if (token_equals(key->token, "minLength")) {
      has_min_len = true;
    } else if (token_equals(key->token, "maxLength")) {
      has_max_len = true;
    } else if (token_equals(key->token, "minItems")) {
      has_min_items = true;
    } else if (token_equals(key->token, "maxItems")) {
      has_max_items = true;
    } else if (token_equals(key->token, "items")) {
      if (val->type == AST_OBJECT || is_ast_true(val) || is_ast_false(val)) {
        has_items = true;
      }
    } else if (token_equals(key->token, "contains")) {
      if (val->type == AST_OBJECT || is_ast_true(val) || is_ast_false(val)) {
        has_contains = true;
      }
    } else if (token_equals(key->token, "not")) {
      if (val->type == AST_OBJECT || is_ast_true(val) || is_ast_false(val)) {
        has_not = true;
      }
    } else if (token_equals(key->token, "allOf") && val->type == AST_ARRAY) {
      has_all_of = true;
      int sub_idx = val->first_child;
      while (sub_idx != -1) {
        all_of_count++;
        sub_idx = nodes[sub_idx].next_sibling;
      }
    } else if (token_equals(key->token, "anyOf") && val->type == AST_ARRAY) {
      has_any_of = true;
      int sub_idx = val->first_child;
      while (sub_idx != -1) {
        any_of_count++;
        sub_idx = nodes[sub_idx].next_sibling;
      }
    } else if (token_equals(key->token, "oneOf") && val->type == AST_ARRAY) {
      has_one_of = true;
      int sub_idx = val->first_child;
      while (sub_idx != -1) {
        one_of_count++;
        sub_idx = nodes[sub_idx].next_sibling;
      }
    } else if (token_equals(key->token, "required") && val->type == AST_ARRAY) {
      int r_idx = val->first_child;
      while (r_idx != -1) {
        required_count++;
        uint32_t len = 0;
        get_stripped_string(nodes[r_idx].token, &len);
        *data_size += len;
        r_idx = nodes[r_idx].next_sibling;
      }
    } else if (token_equals(key->token, "properties") && val->type == AST_OBJECT) {
      int p_idx = val->first_child;
      while (p_idx != -1) {
        prop_count++;
        uint32_t len = 0;
        get_stripped_string(nodes[p_idx].token, &len);
        *data_size += len;
        p_idx = nodes[p_idx + 1].next_sibling;
      }
    } else if (token_equals(key->token, "patternProperties") && val->type == AST_OBJECT) {
      int p_idx = val->first_child;
      while (p_idx != -1) {
        pattern_prop_count++;
        uint32_t len = 0;
        get_stripped_string(nodes[p_idx].token, &len);
        *data_size += len;
        p_idx = nodes[p_idx + 1].next_sibling;
      }
    } else if (token_equals(key->token, "additionalProperties")) {
      has_additional_props = true;
    }

    key_idx = val->next_sibling;
  }

  if (has_type) {
    own_size += 1 + sizeof(uint32_t);
  }
  if (has_min) {
    own_size += 1 + sizeof(double);
  }
  if (has_max) {
    own_size += 1 + sizeof(double);
  }
  if (has_multiple_of) {
    own_size += 1 + sizeof(double);
  }
  if (has_ex_min) {
    own_size += 1 + sizeof(double);
  }
  if (has_ex_max) {
    own_size += 1 + sizeof(double);
  }
  if (has_pattern) {
    own_size += 1 + sizeof(uint32_t) + sizeof(uint32_t);
  }
  if (has_min_props) {
    own_size += 1 + sizeof(int32_t);
  }
  if (has_max_props) {
    own_size += 1 + sizeof(int32_t);
  }
  if (has_unique_items) {
    own_size += 1;
  }
  if (has_min_len) {
    own_size += 1 + sizeof(int32_t);
  }
  if (has_max_len) {
    own_size += 1 + sizeof(int32_t);
  }
  if (has_min_items) {
    own_size += 1 + sizeof(int32_t);
  }
  if (has_max_items) {
    own_size += 1 + sizeof(int32_t);
  }
  if (has_items) {
    own_size += 1 + sizeof(uint32_t);
  }
  if (has_contains) {
    own_size += 1 + sizeof(uint32_t);
  }
  if (has_not) {
    own_size += 1 + sizeof(uint32_t);
  }
  if (has_all_of) {
    own_size += 1 + sizeof(uint32_t) + all_of_count * sizeof(uint32_t);
  }
  if (has_any_of) {
    own_size += 1 + sizeof(uint32_t) + any_of_count * sizeof(uint32_t);
  }
  if (has_one_of) {
    own_size += 1 + sizeof(uint32_t) + one_of_count * sizeof(uint32_t);
  }
  if (has_if) {
    own_size += 1 + 3 * sizeof(uint32_t);
  }
  if (has_property_names) {
    own_size += 1 + sizeof(uint32_t);
  }
  if (has_format) {
    own_size += 1 + sizeof(uint32_t) + sizeof(uint32_t);
  }
  if (required_count > 0) {
    own_size += 1 + sizeof(uint32_t) + required_count * (sizeof(uint32_t) + sizeof(uint32_t));
  }
  if (prop_count > 0 || pattern_prop_count > 0 || has_additional_props) {
    own_size += 1 + sizeof(uint32_t) + sizeof(uint32_t) + sizeof(int32_t)
             + prop_count * (sizeof(uint32_t) * 3)
             + pattern_prop_count * (sizeof(uint32_t) * 3);
  }
  own_size += 1; // OP_END

  uint32_t total_size = own_size;

  // Recursively calculate subschemas in AST order
  key_idx = nodes[node_idx].first_child;
  while (key_idx != -1) {
    ASTNode *key = &nodes[key_idx];
    ASTNode *val = &nodes[key_idx + 1];

    if (token_equals(key->token, "items")) {
      if (val->type == AST_OBJECT || is_ast_true(val) || is_ast_false(val)) {
        uint32_t sub_size = calculate_schema_size(nodes, key_idx + 1, offsets, current_offset + total_size, data_size);
        total_size += sub_size;
      }
    } else if (token_equals(key->token, "contains")) {
      if (val->type == AST_OBJECT || is_ast_true(val) || is_ast_false(val)) {
        uint32_t sub_size = calculate_schema_size(nodes, key_idx + 1, offsets, current_offset + total_size, data_size);
        total_size += sub_size;
      }
    } else if (token_equals(key->token, "not")) {
      if (val->type == AST_OBJECT || is_ast_true(val) || is_ast_false(val)) {
        uint32_t sub_size = calculate_schema_size(nodes, key_idx + 1, offsets, current_offset + total_size, data_size);
        total_size += sub_size;
      }
    } else if (token_equals(key->token, "allOf") && val->type == AST_ARRAY) {
      int sub_idx = val->first_child;
      while (sub_idx != -1) {
        if (nodes[sub_idx].type == AST_OBJECT || is_ast_true(&nodes[sub_idx]) || is_ast_false(&nodes[sub_idx])) {
          uint32_t sub_size = calculate_schema_size(nodes, sub_idx, offsets, current_offset + total_size, data_size);
          total_size += sub_size;
        }
        sub_idx = nodes[sub_idx].next_sibling;
      }
    } else if (token_equals(key->token, "anyOf") && val->type == AST_ARRAY) {
      int sub_idx = val->first_child;
      while (sub_idx != -1) {
        if (nodes[sub_idx].type == AST_OBJECT || is_ast_true(&nodes[sub_idx]) || is_ast_false(&nodes[sub_idx])) {
          uint32_t sub_size = calculate_schema_size(nodes, sub_idx, offsets, current_offset + total_size, data_size);
          total_size += sub_size;
        }
        sub_idx = nodes[sub_idx].next_sibling;
      }
    } else if (token_equals(key->token, "oneOf") && val->type == AST_ARRAY) {
      int sub_idx = val->first_child;
      while (sub_idx != -1) {
        if (nodes[sub_idx].type == AST_OBJECT || is_ast_true(&nodes[sub_idx]) || is_ast_false(&nodes[sub_idx])) {
          uint32_t sub_size = calculate_schema_size(nodes, sub_idx, offsets, current_offset + total_size, data_size);
          total_size += sub_size;
        }
        sub_idx = nodes[sub_idx].next_sibling;
      }
    } else if (token_equals(key->token, "if")) {
      if (val->type == AST_OBJECT || is_ast_true(val) || is_ast_false(val)) {
        uint32_t sub_size = calculate_schema_size(nodes, key_idx + 1, offsets, current_offset + total_size, data_size);
        total_size += sub_size;
      }
    } else if (token_equals(key->token, "then")) {
      if (object_find_key_val_idx(nodes, node_idx, "if") != -1) {
        if (val->type == AST_OBJECT || is_ast_true(val) || is_ast_false(val)) {
          uint32_t sub_size = calculate_schema_size(nodes, key_idx + 1, offsets, current_offset + total_size, data_size);
          total_size += sub_size;
        }
      }
    } else if (token_equals(key->token, "else")) {
      if (object_find_key_val_idx(nodes, node_idx, "if") != -1) {
        if (val->type == AST_OBJECT || is_ast_true(val) || is_ast_false(val)) {
          uint32_t sub_size = calculate_schema_size(nodes, key_idx + 1, offsets, current_offset + total_size, data_size);
          total_size += sub_size;
        }
      }
    } else if (token_equals(key->token, "propertyNames")) {
      if (val->type == AST_OBJECT || is_ast_true(val) || is_ast_false(val)) {
        uint32_t sub_size = calculate_schema_size(nodes, key_idx + 1, offsets, current_offset + total_size, data_size);
        total_size += sub_size;
      }
    } else if (token_equals(key->token, "properties") && val->type == AST_OBJECT) {
      int p_idx = val->first_child;
      while (p_idx != -1) {
        ASTNode *p_val = &nodes[p_idx + 1];
        uint32_t sub_size = calculate_schema_size(nodes, p_idx + 1, offsets, current_offset + total_size, data_size);
        total_size += sub_size;
        p_idx = p_val->next_sibling;
      }
    } else if (token_equals(key->token, "patternProperties") && val->type == AST_OBJECT) {
      int p_idx = val->first_child;
      while (p_idx != -1) {
        ASTNode *p_val = &nodes[p_idx + 1];
        uint32_t sub_size = calculate_schema_size(nodes, p_idx + 1, offsets, current_offset + total_size, data_size);
        total_size += sub_size;
        p_idx = p_val->next_sibling;
      }
    } else if (token_equals(key->token, "additionalProperties")) {
      if (val->type == AST_OBJECT || is_ast_true(val)) {
        uint32_t sub_size = calculate_schema_size(nodes, key_idx + 1, offsets, current_offset + total_size, data_size);
        total_size += sub_size;
      }
    }

    key_idx = val->next_sibling;
  }

  return total_size;
  /*#endregion*/
}

// --- DIRECT COMPILER PASS 2: SERIALIZATION ---

static void serialize_schema_direct(
    ASTNode *nodes,
    int node_idx,
    const uint32_t *offsets,
    uint8_t *bytecode,
    uint32_t *code_write_ptr,
    uint32_t *data_write_ptr,
    uint32_t constant_pool_start
) {
  /*#region*/
  uint8_t *pc = bytecode + *code_write_ptr;

  if (is_ast_false(&nodes[node_idx])) {
    emit_byte(&pc, OP_FAIL);
    *code_write_ptr += 1;
    return;
  }
  if (is_ast_true(&nodes[node_idx]) || nodes[node_idx].type != AST_OBJECT) {
    emit_byte(&pc, OP_END);
    *code_write_ptr += 1;
    return;
  }

  bool has_type = false;
  uint32_t type_mask = 0;
  bool has_min = false;
  double min_val = 0.0;
  bool has_max = false;
  double max_val = 0.0;
  bool has_min_len = false;
  int32_t min_len = -1;
  bool has_max_len = false;
  int32_t max_len = -1;
  bool has_min_items = false;
  int32_t min_items = -1;
  bool has_max_items = false;
  int32_t max_items = -1;
  bool has_multiple_of = false;
  double multiple_of_val = 0.0;
  bool has_ex_min = false;
  double ex_min_val = 0.0;
  bool has_ex_max = false;
  double ex_max_val = 0.0;
  bool has_pattern = false;
  Token pattern_token = {0};
  bool has_min_props = false;
  int32_t min_props = -1;
  bool has_max_props = false;
  int32_t max_props = -1;
  bool has_unique_items = false;
  bool has_items = false;
  int items_val_idx = -1;
  bool has_contains = false;
  int contains_val_idx = -1;
  bool has_not = false;
  int not_val_idx = -1;
  bool has_all_of = false;
  int all_of_val_idx = -1;
  int all_of_count = 0;
  bool has_any_of = false;
  int any_of_val_idx = -1;
  int any_of_count = 0;
  bool has_one_of = false;
  int one_of_val_idx = -1;
  int one_of_count = 0;
  bool has_if = false;
  int if_val_idx = -1;
  int then_val_idx = -1;
  int else_val_idx = -1;
  bool has_property_names = false;
  int property_names_val_idx = -1;
  int pattern_prop_count = 0;
  int pattern_properties_val_idx = -1;
  bool has_format = false;
  Token format_token = {0};
  int required_count = 0;
  int required_val_idx = -1;
  int prop_count = 0;
  int properties_val_idx = -1;
  bool has_additional_props = false;
  int additional_props_val_idx = -1;

  int key_idx = nodes[node_idx].first_child;
  while (key_idx != -1) {
    ASTNode *key = &nodes[key_idx];
    ASTNode *val = &nodes[key_idx + 1];

    if (token_equals(key->token, "type")) {
      has_type = true;
      if (is_ast_string(val)) {
        type_mask = map_type_string_to_mask(val->token);
      } else if (val->type == AST_ARRAY) {
        int t_idx = val->first_child;
        while (t_idx != -1) {
          type_mask |= map_type_string_to_mask(nodes[t_idx].token);
          t_idx = nodes[t_idx].next_sibling;
        }
      }
    } else if (token_equals(key->token, "minimum")) {
      has_min = true;
      min_val = parse_number(val->token);
    } else if (token_equals(key->token, "maximum")) {
      has_max = true;
      max_val = parse_number(val->token);
    } else if (token_equals(key->token, "multipleOf")) {
      has_multiple_of = true;
      multiple_of_val = parse_number(val->token);
    } else if (token_equals(key->token, "exclusiveMinimum")) {
      has_ex_min = true;
      ex_min_val = parse_number(val->token);
    } else if (token_equals(key->token, "exclusiveMaximum")) {
      has_ex_max = true;
      ex_max_val = parse_number(val->token);
    } else if (token_equals(key->token, "pattern")) {
      has_pattern = true;
      pattern_token = val->token;
    } else if (token_equals(key->token, "minProperties")) {
      has_min_props = true;
      min_props = (int32_t)parse_number(val->token);
    } else if (token_equals(key->token, "maxProperties")) {
      has_max_props = true;
      max_props = (int32_t)parse_number(val->token);
    } else if (token_equals(key->token, "uniqueItems")) {
      has_unique_items = is_ast_true(val);
    } else if (token_equals(key->token, "minLength")) {
      has_min_len = true;
      min_len = (int32_t)parse_number(val->token);
    } else if (token_equals(key->token, "maxLength")) {
      has_max_len = true;
      max_len = (int32_t)parse_number(val->token);
    } else if (token_equals(key->token, "minItems")) {
      has_min_items = true;
      min_items = (int32_t)parse_number(val->token);
    } else if (token_equals(key->token, "maxItems")) {
      has_max_items = true;
      max_items = (int32_t)parse_number(val->token);
    } else if (token_equals(key->token, "items")) {
      if (val->type == AST_OBJECT || is_ast_true(val) || is_ast_false(val)) {
        has_items = true;
        items_val_idx = key_idx + 1;
      }
    } else if (token_equals(key->token, "contains")) {
      if (val->type == AST_OBJECT || is_ast_true(val) || is_ast_false(val)) {
        has_contains = true;
        contains_val_idx = key_idx + 1;
      }
    } else if (token_equals(key->token, "not")) {
      if (val->type == AST_OBJECT || is_ast_true(val) || is_ast_false(val)) {
        has_not = true;
        not_val_idx = key_idx + 1;
      }
    } else if (token_equals(key->token, "allOf") && val->type == AST_ARRAY) {
      has_all_of = true;
      all_of_val_idx = key_idx + 1;
      int sub_idx = val->first_child;
      while (sub_idx != -1) {
        all_of_count++;
        sub_idx = nodes[sub_idx].next_sibling;
      }
    } else if (token_equals(key->token, "anyOf") && val->type == AST_ARRAY) {
      has_any_of = true;
      any_of_val_idx = key_idx + 1;
      int sub_idx = val->first_child;
      while (sub_idx != -1) {
        any_of_count++;
        sub_idx = nodes[sub_idx].next_sibling;
      }
    } else if (token_equals(key->token, "oneOf") && val->type == AST_ARRAY) {
      has_one_of = true;
      one_of_val_idx = key_idx + 1;
      int sub_idx = val->first_child;
      while (sub_idx != -1) {
        one_of_count++;
        sub_idx = nodes[sub_idx].next_sibling;
      }
    } else if (token_equals(key->token, "if")) {
      if (val->type == AST_OBJECT || is_ast_true(val) || is_ast_false(val)) {
        has_if = true;
        if_val_idx = key_idx + 1;
      }
    } else if (token_equals(key->token, "propertyNames")) {
      if (val->type == AST_OBJECT || is_ast_true(val) || is_ast_false(val)) {
        has_property_names = true;
        property_names_val_idx = key_idx + 1;
      }
    } else if (token_equals(key->token, "format")) {
      has_format = true;
      format_token = val->token;
    } else if (token_equals(key->token, "then")) {
      if (val->type == AST_OBJECT || is_ast_true(val) || is_ast_false(val)) {
        then_val_idx = key_idx + 1;
      }
    } else if (token_equals(key->token, "else")) {
      if (val->type == AST_OBJECT || is_ast_true(val) || is_ast_false(val)) {
        else_val_idx = key_idx + 1;
      }
    } else if (token_equals(key->token, "required") && val->type == AST_ARRAY) {
      required_val_idx = key_idx + 1;
      int r_idx = val->first_child;
      while (r_idx != -1) {
        required_count++;
        r_idx = nodes[r_idx].next_sibling;
      }
    } else if (token_equals(key->token, "properties") && val->type == AST_OBJECT) {
      properties_val_idx = key_idx + 1;
      int p_idx = val->first_child;
      while (p_idx != -1) {
        prop_count++;
        p_idx = nodes[p_idx + 1].next_sibling;
      }
    } else if (token_equals(key->token, "patternProperties") && val->type == AST_OBJECT) {
      pattern_properties_val_idx = key_idx + 1;
      int p_idx = val->first_child;
      while (p_idx != -1) {
        pattern_prop_count++;
        p_idx = nodes[p_idx + 1].next_sibling;
      }
    } else if (token_equals(key->token, "additionalProperties")) {
      has_additional_props = true;
      additional_props_val_idx = key_idx + 1;
    }

    key_idx = val->next_sibling;
  }

  if (has_type) {
    emit_byte(&pc, OP_TYPE);
    emit_uint32(&pc, type_mask);
  }
  if (has_min) {
    emit_byte(&pc, OP_MINIMUM);
    emit_double(&pc, min_val);
  }
  if (has_max) {
    emit_byte(&pc, OP_MAXIMUM);
    emit_double(&pc, max_val);
  }
  if (has_multiple_of) {
    emit_byte(&pc, OP_MULTIPLE_OF);
    emit_double(&pc, multiple_of_val);
  }
  if (has_ex_min) {
    emit_byte(&pc, OP_EXCLUSIVE_MINIMUM);
    emit_double(&pc, ex_min_val);
  }
  if (has_ex_max) {
    emit_byte(&pc, OP_EXCLUSIVE_MAXIMUM);
    emit_double(&pc, ex_max_val);
  }
  if (has_pattern) {
    emit_byte(&pc, OP_PATTERN);
    emit_string_ref(&pc, bytecode, constant_pool_start, data_write_ptr, pattern_token);
  }
  if (has_min_props) {
    emit_byte(&pc, OP_MIN_PROPERTIES);
    emit_int32(&pc, min_props);
  }
  if (has_max_props) {
    emit_byte(&pc, OP_MAX_PROPERTIES);
    emit_int32(&pc, max_props);
  }
  if (has_unique_items) {
    emit_byte(&pc, OP_UNIQUE_ITEMS);
  }
  if (has_min_len) {
    emit_byte(&pc, OP_MIN_LENGTH);
    emit_int32(&pc, min_len);
  }
  if (has_max_len) {
    emit_byte(&pc, OP_MAX_LENGTH);
    emit_int32(&pc, max_len);
  }
  if (has_min_items) {
    emit_byte(&pc, OP_MIN_ITEMS);
    emit_int32(&pc, min_items);
  }
  if (has_max_items) {
    emit_byte(&pc, OP_MAX_ITEMS);
    emit_int32(&pc, max_items);
  }
  if (has_items) {
    emit_byte(&pc, OP_ITEMS);
    emit_uint32(&pc, offsets[items_val_idx]);
  }
  if (has_contains) {
    emit_byte(&pc, OP_CONTAINS);
    emit_uint32(&pc, offsets[contains_val_idx]);
  }
  if (has_not) {
    emit_byte(&pc, OP_NOT);
    emit_uint32(&pc, offsets[not_val_idx]);
  }
  if (has_all_of && all_of_val_idx != -1) {
    emit_byte(&pc, OP_ALL_OF);
    emit_uint32(&pc, (uint32_t)all_of_count);
    int sub_idx = nodes[all_of_val_idx].first_child;
    while (sub_idx != -1) {
      emit_uint32(&pc, offsets[sub_idx]);
      sub_idx = nodes[sub_idx].next_sibling;
    }
  }
  if (has_any_of && any_of_val_idx != -1) {
    emit_byte(&pc, OP_ANY_OF);
    emit_uint32(&pc, (uint32_t)any_of_count);
    int sub_idx = nodes[any_of_val_idx].first_child;
    while (sub_idx != -1) {
      emit_uint32(&pc, offsets[sub_idx]);
      sub_idx = nodes[sub_idx].next_sibling;
    }
  }
  if (has_one_of && one_of_val_idx != -1) {
    emit_byte(&pc, OP_ONE_OF);
    emit_uint32(&pc, (uint32_t)one_of_count);
    int sub_idx = nodes[one_of_val_idx].first_child;
    while (sub_idx != -1) {
      emit_uint32(&pc, offsets[sub_idx]);
      sub_idx = nodes[sub_idx].next_sibling;
    }
  }
  if (has_if && if_val_idx != -1) {
    emit_byte(&pc, OP_IF_THEN_ELSE);
    emit_uint32(&pc, offsets[if_val_idx]);
    emit_uint32(&pc, (then_val_idx != -1) ? offsets[then_val_idx] : (uint32_t)-1);
    emit_uint32(&pc, (else_val_idx != -1) ? offsets[else_val_idx] : (uint32_t)-1);
  }
  if (has_property_names && property_names_val_idx != -1) {
    emit_byte(&pc, OP_PROPERTY_NAMES);
    emit_uint32(&pc, offsets[property_names_val_idx]);
  }
  if (has_format) {
    emit_byte(&pc, OP_FORMAT);
    emit_string_ref(&pc, bytecode, constant_pool_start, data_write_ptr, format_token);
  }
  if (required_count > 0 && required_val_idx != -1) {
    emit_byte(&pc, OP_REQUIRED);
    emit_uint32(&pc, (uint32_t)required_count);
    int r_idx = nodes[required_val_idx].first_child;
    while (r_idx != -1) {
      emit_string_ref(&pc, bytecode, constant_pool_start, data_write_ptr, nodes[r_idx].token);
      r_idx = nodes[r_idx].next_sibling;
    }
  }
  if (prop_count > 0 || pattern_prop_count > 0 || has_additional_props) {
    emit_byte(&pc, OP_PROPERTIES);
    emit_uint32(&pc, (uint32_t)prop_count);
    emit_uint32(&pc, (uint32_t)pattern_prop_count);
    int32_t add_rule = -1;
    if (has_additional_props && additional_props_val_idx != -1) {
      ASTNode *add_val = &nodes[additional_props_val_idx];
      if (is_ast_false(add_val)) {
        add_rule = -2;
      } else if (add_val->type == AST_OBJECT || is_ast_true(add_val)) {
        add_rule = (int32_t)offsets[additional_props_val_idx];
      }
    }
    emit_int32(&pc, add_rule);

    if (prop_count > 0 && properties_val_idx != -1) {
      int p_idx = nodes[properties_val_idx].first_child;
      while (p_idx != -1) {
        ASTNode *p_key = &nodes[p_idx];
        emit_string_ref(&pc, bytecode, constant_pool_start, data_write_ptr, p_key->token);
        emit_uint32(&pc, offsets[p_idx + 1]);
        p_idx = nodes[p_idx + 1].next_sibling;
      }
    }

    if (pattern_prop_count > 0 && pattern_properties_val_idx != -1) {
      int p_idx = nodes[pattern_properties_val_idx].first_child;
      while (p_idx != -1) {
        ASTNode *p_key = &nodes[p_idx];
        emit_string_ref(&pc, bytecode, constant_pool_start, data_write_ptr, p_key->token);
        emit_uint32(&pc, offsets[p_idx + 1]);
        p_idx = nodes[p_idx + 1].next_sibling;
      }
    }
  }
  emit_byte(&pc, OP_END);

  uint32_t own_written = (uint32_t)(pc - (bytecode + *code_write_ptr));
  *code_write_ptr += own_written;

  // Recursively serialize subschemas in AST order
  key_idx = nodes[node_idx].first_child;
  while (key_idx != -1) {
    ASTNode *key = &nodes[key_idx];
    ASTNode *val = &nodes[key_idx + 1];

    if (token_equals(key->token, "items")) {
      if (val->type == AST_OBJECT || is_ast_true(val) || is_ast_false(val)) {
        serialize_schema_direct(nodes, key_idx + 1, offsets, bytecode, code_write_ptr, data_write_ptr, constant_pool_start);
      }
    } else if (token_equals(key->token, "contains")) {
      if (val->type == AST_OBJECT || is_ast_true(val) || is_ast_false(val)) {
        serialize_schema_direct(nodes, key_idx + 1, offsets, bytecode, code_write_ptr, data_write_ptr, constant_pool_start);
      }
    } else if (token_equals(key->token, "not")) {
      if (val->type == AST_OBJECT || is_ast_true(val) || is_ast_false(val)) {
        serialize_schema_direct(nodes, key_idx + 1, offsets, bytecode, code_write_ptr, data_write_ptr, constant_pool_start);
      }
    } else if (token_equals(key->token, "allOf") && val->type == AST_ARRAY) {
      int sub_idx = val->first_child;
      while (sub_idx != -1) {
        if (nodes[sub_idx].type == AST_OBJECT || is_ast_true(&nodes[sub_idx]) || is_ast_false(&nodes[sub_idx])) {
          serialize_schema_direct(nodes, sub_idx, offsets, bytecode, code_write_ptr, data_write_ptr, constant_pool_start);
        }
        sub_idx = nodes[sub_idx].next_sibling;
      }
    } else if (token_equals(key->token, "anyOf") && val->type == AST_ARRAY) {
      int sub_idx = val->first_child;
      while (sub_idx != -1) {
        if (nodes[sub_idx].type == AST_OBJECT || is_ast_true(&nodes[sub_idx]) || is_ast_false(&nodes[sub_idx])) {
          serialize_schema_direct(nodes, sub_idx, offsets, bytecode, code_write_ptr, data_write_ptr, constant_pool_start);
        }
        sub_idx = nodes[sub_idx].next_sibling;
      }
    } else if (token_equals(key->token, "oneOf") && val->type == AST_ARRAY) {
      int sub_idx = val->first_child;
      while (sub_idx != -1) {
        if (nodes[sub_idx].type == AST_OBJECT || is_ast_true(&nodes[sub_idx]) || is_ast_false(&nodes[sub_idx])) {
          serialize_schema_direct(nodes, sub_idx, offsets, bytecode, code_write_ptr, data_write_ptr, constant_pool_start);
        }
        sub_idx = nodes[sub_idx].next_sibling;
      }
    } else if (token_equals(key->token, "if")) {
      if (val->type == AST_OBJECT || is_ast_true(val) || is_ast_false(val)) {
        serialize_schema_direct(nodes, key_idx + 1, offsets, bytecode, code_write_ptr, data_write_ptr, constant_pool_start);
      }
    } else if (token_equals(key->token, "then")) {
      if (object_find_key_val_idx(nodes, node_idx, "if") != -1) {
        if (val->type == AST_OBJECT || is_ast_true(val) || is_ast_false(val)) {
          serialize_schema_direct(nodes, key_idx + 1, offsets, bytecode, code_write_ptr, data_write_ptr, constant_pool_start);
        }
      }
    } else if (token_equals(key->token, "else")) {
      if (object_find_key_val_idx(nodes, node_idx, "if") != -1) {
        if (val->type == AST_OBJECT || is_ast_true(val) || is_ast_false(val)) {
          serialize_schema_direct(nodes, key_idx + 1, offsets, bytecode, code_write_ptr, data_write_ptr, constant_pool_start);
        }
      }
    } else if (token_equals(key->token, "propertyNames")) {
      if (val->type == AST_OBJECT || is_ast_true(val) || is_ast_false(val)) {
        serialize_schema_direct(nodes, key_idx + 1, offsets, bytecode, code_write_ptr, data_write_ptr, constant_pool_start);
      }
    } else if (token_equals(key->token, "properties") && val->type == AST_OBJECT) {
      int p_idx = val->first_child;
      while (p_idx != -1) {
        ASTNode *p_val = &nodes[p_idx + 1];
        serialize_schema_direct(nodes, p_idx + 1, offsets, bytecode, code_write_ptr, data_write_ptr, constant_pool_start);
        p_idx = p_val->next_sibling;
      }
    } else if (token_equals(key->token, "patternProperties") && val->type == AST_OBJECT) {
      int p_idx = val->first_child;
      while (p_idx != -1) {
        ASTNode *p_val = &nodes[p_idx + 1];
        serialize_schema_direct(nodes, p_idx + 1, offsets, bytecode, code_write_ptr, data_write_ptr, constant_pool_start);
        p_idx = p_val->next_sibling;
      }
    } else if (token_equals(key->token, "additionalProperties")) {
      if (val->type == AST_OBJECT || is_ast_true(val)) {
        serialize_schema_direct(nodes, key_idx + 1, offsets, bytecode, code_write_ptr, data_write_ptr, constant_pool_start);
      }
    }

    key_idx = val->next_sibling;
  }
  /*#endregion*/
}

// --- PUBLIC API ---

uint8_t *compile_schema(Jsonv_Arena *arena, ASTNode *nodes, int ast_count, int root_idx, int *out_length) {
  /*#region*/
  if (ast_count <= 0) {
    *out_length = 0;
    return NULL;
  }

  uint32_t *offsets = (uint32_t *)jsonv_arena_alloc(arena, ast_count * sizeof(uint32_t));
  if (!offsets) {
    *out_length = 0;
    return NULL;
  }
  for (int i = 0; i < ast_count; i++) {
    offsets[i] = (uint32_t)-1;
  }

  uint32_t data_size = 0;
  uint32_t code_size = calculate_schema_size(nodes, root_idx, offsets, sizeof(BytecodeHeader), &data_size);
  uint32_t total_size = sizeof(BytecodeHeader) + code_size + data_size;

  uint8_t *bytecode = (uint8_t *)jsonv_arena_alloc(arena, total_size);
  if (!bytecode) {
    *out_length = 0;
    return NULL;
  }

  BytecodeHeader header;
  header.magic = 0x4A535642;
  header.version = 1;
  header.code_size = code_size;
  header.data_size = data_size;
  memcpy(bytecode, &header, sizeof(BytecodeHeader));

  uint32_t code_write_ptr = sizeof(BytecodeHeader);
  uint32_t data_write_ptr = sizeof(BytecodeHeader) + code_size;
  uint32_t constant_pool_start = sizeof(BytecodeHeader) + code_size;

  serialize_schema_direct(nodes, root_idx, offsets, bytecode, &code_write_ptr, &data_write_ptr, constant_pool_start);

  *out_length = (int)total_size;
  return bytecode;
  /*#endregion*/
}

// --- DEBUGGING ---

void print_token(Token t) {
  /*#region*/
  if (t.type == T_STRING) {
    const char *start = (const char *)t.value.string.start;
    size_t len = t.value.string.length;
    if (len >= 2 && start[0] == '"' && start[len - 1] == '"') {
      printf("%.*s", (int)len, start);
    } else {
      printf("\"%.*s\"", (int)len, start);
    }
  } else if (t.type == T_NUMBER) {
    printf("%g", t.value.number);
  } else if (t.type == T_TRUE) {
    printf("true");
  } else if (t.type == T_FALSE) {
    printf("false");
  } else if (t.type == T_NULL) {
    printf("null");
  }
  /*#endregion*/
}
