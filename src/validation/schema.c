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

static inline void emit_token(uint8_t **pc, Token val) {
  /*#region*/
  memcpy(*pc, &val, sizeof(val));
  *pc += sizeof(val);
  /*#endregion*/
}

// --- DIRECT COMPILER PASS 1: SIZE CALCULATION ---

static uint32_t calculate_schema_size(ASTNode *nodes, int node_idx, uint32_t *offsets, uint32_t current_offset) {
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
    } else if (token_equals(key->token, "minProperties")) {
      has_min_props = true;
    } else if (token_equals(key->token, "maxProperties")) {
      has_max_props = true;
    } else if (token_equals(key->token, "uniqueItems")) {
      has_unique_items = is_ast_true(val);
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
    } else if (token_equals(key->token, "required") && val->type == AST_ARRAY) {
      int r_idx = val->first_child;
      while (r_idx != -1) {
        required_count++;
        r_idx = nodes[r_idx].next_sibling;
      }
    } else if (token_equals(key->token, "properties") && val->type == AST_OBJECT) {
      int p_idx = val->first_child;
      while (p_idx != -1) {
        prop_count++;
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
    own_size += 1 + sizeof(Token);
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
  if (required_count > 0) {
    own_size += 1 + sizeof(uint32_t) + required_count * sizeof(Token);
  }
  if (prop_count > 0 || has_additional_props) {
    own_size += 1 + sizeof(uint32_t) + sizeof(int32_t) + prop_count * (sizeof(Token) + sizeof(uint32_t));
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
        uint32_t sub_size = calculate_schema_size(nodes, key_idx + 1, offsets, current_offset + total_size);
        total_size += sub_size;
      }
    } else if (token_equals(key->token, "contains")) {
      if (val->type == AST_OBJECT || is_ast_true(val) || is_ast_false(val)) {
        uint32_t sub_size = calculate_schema_size(nodes, key_idx + 1, offsets, current_offset + total_size);
        total_size += sub_size;
      }
    } else if (token_equals(key->token, "not")) {
      if (val->type == AST_OBJECT || is_ast_true(val) || is_ast_false(val)) {
        uint32_t sub_size = calculate_schema_size(nodes, key_idx + 1, offsets, current_offset + total_size);
        total_size += sub_size;
      }
    } else if (token_equals(key->token, "allOf") && val->type == AST_ARRAY) {
      int sub_idx = val->first_child;
      while (sub_idx != -1) {
        if (nodes[sub_idx].type == AST_OBJECT || is_ast_true(&nodes[sub_idx]) || is_ast_false(&nodes[sub_idx])) {
          uint32_t sub_size = calculate_schema_size(nodes, sub_idx, offsets, current_offset + total_size);
          total_size += sub_size;
        }
        sub_idx = nodes[sub_idx].next_sibling;
      }
    } else if (token_equals(key->token, "properties") && val->type == AST_OBJECT) {
      int p_idx = val->first_child;
      while (p_idx != -1) {
        ASTNode *p_val = &nodes[p_idx + 1];
        uint32_t sub_size = calculate_schema_size(nodes, p_idx + 1, offsets, current_offset + total_size);
        total_size += sub_size;
        p_idx = p_val->next_sibling;
      }
    } else if (token_equals(key->token, "additionalProperties")) {
      if (val->type == AST_OBJECT || is_ast_true(val)) {
        uint32_t sub_size = calculate_schema_size(nodes, key_idx + 1, offsets, current_offset + total_size);
        total_size += sub_size;
      }
    }

    key_idx = val->next_sibling;
  }

  return total_size;
  /*#endregion*/
}

// --- DIRECT COMPILER PASS 2: SERIALIZATION ---

static void serialize_schema_direct(ASTNode *nodes, int node_idx, const uint32_t *offsets, uint8_t *bytecode, uint32_t *write_ptr) {
  /*#region*/
  uint8_t *pc = bytecode + *write_ptr;

  if (is_ast_false(&nodes[node_idx])) {
    emit_byte(&pc, OP_FAIL);
    *write_ptr += 1;
    return;
  }
  if (is_ast_true(&nodes[node_idx]) || nodes[node_idx].type != AST_OBJECT) {
    emit_byte(&pc, OP_END);
    *write_ptr += 1;
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
    emit_token(&pc, pattern_token);
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
  if (required_count > 0 && required_val_idx != -1) {
    emit_byte(&pc, OP_REQUIRED);
    emit_uint32(&pc, (uint32_t)required_count);
    int r_idx = nodes[required_val_idx].first_child;
    while (r_idx != -1) {
      emit_token(&pc, nodes[r_idx].token);
      r_idx = nodes[r_idx].next_sibling;
    }
  }
  if (prop_count > 0 || has_additional_props) {
    emit_byte(&pc, OP_PROPERTIES);
    emit_uint32(&pc, (uint32_t)prop_count);
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
        emit_token(&pc, p_key->token);
        emit_uint32(&pc, offsets[p_idx + 1]);
        p_idx = nodes[p_idx + 1].next_sibling;
      }
    }
  }
  emit_byte(&pc, OP_END);

  uint32_t own_written = (uint32_t)(pc - (bytecode + *write_ptr));
  *write_ptr += own_written;

  // Recursively serialize subschemas in AST order
  key_idx = nodes[node_idx].first_child;
  while (key_idx != -1) {
    ASTNode *key = &nodes[key_idx];
    ASTNode *val = &nodes[key_idx + 1];

    if (token_equals(key->token, "items")) {
      if (val->type == AST_OBJECT || is_ast_true(val) || is_ast_false(val)) {
        serialize_schema_direct(nodes, key_idx + 1, offsets, bytecode, write_ptr);
      }
    } else if (token_equals(key->token, "contains")) {
      if (val->type == AST_OBJECT || is_ast_true(val) || is_ast_false(val)) {
        serialize_schema_direct(nodes, key_idx + 1, offsets, bytecode, write_ptr);
      }
    } else if (token_equals(key->token, "not")) {
      if (val->type == AST_OBJECT || is_ast_true(val) || is_ast_false(val)) {
        serialize_schema_direct(nodes, key_idx + 1, offsets, bytecode, write_ptr);
      }
    } else if (token_equals(key->token, "allOf") && val->type == AST_ARRAY) {
      int sub_idx = val->first_child;
      while (sub_idx != -1) {
        if (nodes[sub_idx].type == AST_OBJECT || is_ast_true(&nodes[sub_idx]) || is_ast_false(&nodes[sub_idx])) {
          serialize_schema_direct(nodes, sub_idx, offsets, bytecode, write_ptr);
        }
        sub_idx = nodes[sub_idx].next_sibling;
      }
    } else if (token_equals(key->token, "properties") && val->type == AST_OBJECT) {
      int p_idx = val->first_child;
      while (p_idx != -1) {
        ASTNode *p_val = &nodes[p_idx + 1];
        serialize_schema_direct(nodes, p_idx + 1, offsets, bytecode, write_ptr);
        p_idx = p_val->next_sibling;
      }
    } else if (token_equals(key->token, "additionalProperties")) {
      if (val->type == AST_OBJECT || is_ast_true(val)) {
        serialize_schema_direct(nodes, key_idx + 1, offsets, bytecode, write_ptr);
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

  uint32_t total_size = calculate_schema_size(nodes, root_idx, offsets, 0);

  uint8_t *bytecode = (uint8_t *)jsonv_arena_alloc(arena, total_size);
  if (!bytecode) {
    *out_length = 0;
    return NULL;
  }

  uint32_t write_ptr = 0;
  serialize_schema_direct(nodes, root_idx, offsets, bytecode, &write_ptr);

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
