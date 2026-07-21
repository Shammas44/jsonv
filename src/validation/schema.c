#include "schema.h"
#include "parser.h"
#include "arena.internal.h"
#include "atom.h"
#include "table.h"
#include "prescan.h"
#include "except.h"
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

static bool token_equals(Token t, const char *str) {
  /*#region*/
  if (t.type != T_STRING)
    return false;

  const char *t_start = (const char *)t.value.string.start;
  size_t t_len = t.value.string.length;

  size_t str_len = strlen(str);
  if (t_len != str_len)
    return false;

  return strncmp(t_start, str, str_len) == 0;
  /*#endregion*/
}

static inline double parse_number(Token t) {
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

typedef struct {
  const char *name;
  int mask;
} TypeMapping;

static Table *type_table = NULL;

static const TypeMapping type_mappings[] = {
  {"string", TYPE_STRING},
  {"number", TYPE_NUMBER},
  {"integer", TYPE_INTEGER},
  {"boolean", TYPE_BOOL},
  {"object", TYPE_OBJECT},
  {"array", TYPE_ARRAY},
  {"null", TYPE_NULL}
};

#define TYPE_MAPPINGS_COUNT (sizeof(type_mappings) / sizeof(type_mappings[0]))
static const char *type_atoms[TYPE_MAPPINGS_COUNT] = {NULL};

static int map_type_string_to_mask(Token t) {
  /*#region*/
  if (!type_table)
    return 0;

  if (t.type != T_STRING)
    return 0;

  const char *t_start = (const char *)t.value.string.start;
  size_t t_len = t.value.string.length;

  const char *atom_val = atom_new(t_start, (int)t_len);
  void *val = table_get(type_table, atom_val);
  if (val) {
    return (int)(uintptr_t)val;
  }
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

typedef enum {
  KW_NONE = 0,
  KW_SIMPLE,
  KW_RECURSIVE,
  KW_STRING_REF,
  KW_ARRAY_OF,
  KW_IF,
  KW_THEN,
  KW_ELSE,
  KW_REQUIRED,
  KW_PROPERTIES,
  KW_PATTERN_PROPS,
  KW_ADDITIONAL_PROPS
} KeywordType;

typedef enum {
  KWID_TYPE = 0,
  KWID_MINIMUM,
  KWID_MAXIMUM,
  KWID_MULTIPLE_OF,
  KWID_EXCLUSIVE_MINIMUM,
  KWID_EXCLUSIVE_MAXIMUM,
  KWID_MIN_PROPERTIES,
  KWID_MAX_PROPERTIES,
  KWID_UNIQUE_ITEMS,
  KWID_MIN_LENGTH,
  KWID_MAX_LENGTH,
  KWID_MIN_ITEMS,
  KWID_MAX_ITEMS,
  KWID_ITEMS,
  KWID_CONTAINS,
  KWID_NOT,
  KWID_PROPERTY_NAMES,
  KWID_PATTERN,
  KWID_FORMAT,
  KWID_ALL_OF,
  KWID_ANY_OF,
  KWID_ONE_OF,
  KWID_IF,
  KWID_THEN,
  KWID_ELSE,
  KWID_REQUIRED,
  KWID_PROPERTIES,
  KWID_PATTERN_PROPERTIES,
  KWID_ADDITIONAL_PROPERTIES,
  KWID_COUNT
} KeywordID;

typedef struct {
  KeywordType type;
  uint8_t opcode;
  uint32_t base_size;
  KeywordID id;
} KeywordConfig;

static KeywordConfig kw_type                = { KW_SIMPLE,          OP_TYPE,              1 + sizeof(uint32_t), KWID_TYPE };
static KeywordConfig kw_minimum             = { KW_SIMPLE,          OP_MINIMUM,           1 + sizeof(double), KWID_MINIMUM };
static KeywordConfig kw_maximum             = { KW_SIMPLE,          OP_MAXIMUM,           1 + sizeof(double), KWID_MAXIMUM };
static KeywordConfig kw_multiple_of         = { KW_SIMPLE,          OP_MULTIPLE_OF,       1 + sizeof(double), KWID_MULTIPLE_OF };
static KeywordConfig kw_exclusive_min       = { KW_SIMPLE,          OP_EXCLUSIVE_MINIMUM, 1 + sizeof(double), KWID_EXCLUSIVE_MINIMUM };
static KeywordConfig kw_exclusive_max       = { KW_SIMPLE,          OP_EXCLUSIVE_MAXIMUM, 1 + sizeof(double), KWID_EXCLUSIVE_MAXIMUM };
static KeywordConfig kw_min_props           = { KW_SIMPLE,          OP_MIN_PROPERTIES,    1 + sizeof(int32_t), KWID_MIN_PROPERTIES };
static KeywordConfig kw_max_props           = { KW_SIMPLE,          OP_MAX_PROPERTIES,    1 + sizeof(int32_t), KWID_MAX_PROPERTIES };
static KeywordConfig kw_unique_items        = { KW_SIMPLE,          OP_UNIQUE_ITEMS,      1, KWID_UNIQUE_ITEMS };
static KeywordConfig kw_min_length          = { KW_SIMPLE,          OP_MIN_LENGTH,        1 + sizeof(int32_t), KWID_MIN_LENGTH };
static KeywordConfig kw_max_length          = { KW_SIMPLE,          OP_MAX_LENGTH,        1 + sizeof(int32_t), KWID_MAX_LENGTH };
static KeywordConfig kw_min_items           = { KW_SIMPLE,          OP_MIN_ITEMS,         1 + sizeof(int32_t), KWID_MIN_ITEMS };
static KeywordConfig kw_max_items           = { KW_SIMPLE,          OP_MAX_ITEMS,         1 + sizeof(int32_t), KWID_MAX_ITEMS };

static KeywordConfig kw_items               = { KW_RECURSIVE,       OP_ITEMS,             1 + sizeof(uint32_t), KWID_ITEMS };
static KeywordConfig kw_contains            = { KW_RECURSIVE,       OP_CONTAINS,          1 + sizeof(uint32_t), KWID_CONTAINS };
static KeywordConfig kw_not                 = { KW_RECURSIVE,       OP_NOT,               1 + sizeof(uint32_t), KWID_NOT };
static KeywordConfig kw_property_names      = { KW_RECURSIVE,       OP_PROPERTY_NAMES,    1 + sizeof(uint32_t), KWID_PROPERTY_NAMES };

static KeywordConfig kw_pattern             = { KW_STRING_REF,      OP_PATTERN,           1 + sizeof(uint32_t) * 2, KWID_PATTERN };
static KeywordConfig kw_format              = { KW_STRING_REF,      OP_FORMAT,            1 + sizeof(uint32_t) * 2, KWID_FORMAT };

static KeywordConfig kw_all_of              = { KW_ARRAY_OF,        OP_ALL_OF,            1 + sizeof(uint32_t), KWID_ALL_OF };
static KeywordConfig kw_any_of              = { KW_ARRAY_OF,        OP_ANY_OF,            1 + sizeof(uint32_t), KWID_ANY_OF };
static KeywordConfig kw_one_of              = { KW_ARRAY_OF,        OP_ONE_OF,            1 + sizeof(uint32_t), KWID_ONE_OF };

static KeywordConfig kw_if                  = { KW_IF,              OP_IF_THEN_ELSE,      1 + sizeof(uint32_t) * 3, KWID_IF };
static KeywordConfig kw_then                = { KW_THEN,            0,                    0, KWID_THEN };
static KeywordConfig kw_else                = { KW_ELSE,            0,                    0, KWID_ELSE };

static KeywordConfig kw_required            = { KW_REQUIRED,        OP_REQUIRED,          1 + sizeof(uint32_t), KWID_REQUIRED };

static KeywordConfig kw_properties          = { KW_PROPERTIES,      OP_PROPERTIES,        0, KWID_PROPERTIES };
static KeywordConfig kw_pattern_properties  = { KW_PATTERN_PROPS,   OP_PROPERTIES,        0, KWID_PATTERN_PROPERTIES };
static KeywordConfig kw_additional_props    = { KW_ADDITIONAL_PROPS,OP_PROPERTIES,        0, KWID_ADDITIONAL_PROPERTIES };

static Table *keyword_table = NULL;
static const char *kw_atoms[KWID_COUNT] = {NULL};
static const char *kw_strings[KWID_COUNT] = {
  "type", "minimum", "maximum", "multipleOf", "exclusiveMinimum", "exclusiveMaximum",
  "minProperties", "maxProperties", "uniqueItems", "minLength", "maxLength",
  "minItems", "maxItems", "items", "contains", "not", "propertyNames",
  "pattern", "format", "allOf", "anyOf", "oneOf", "if", "then", "else",
  "required", "properties", "patternProperties", "additionalProperties"
};

static void init_keyword_table(void) {
  /*#region*/
  if (keyword_table) return;

  keyword_table = table_new(32, NULL, NULL);
  for (int i = 0; i < KWID_COUNT; i++) {
    kw_atoms[i] = atom_string(kw_strings[i]);
  }

  table_put(keyword_table, kw_atoms[KWID_TYPE], &kw_type);
  table_put(keyword_table, kw_atoms[KWID_MINIMUM], &kw_minimum);
  table_put(keyword_table, kw_atoms[KWID_MAXIMUM], &kw_maximum);
  table_put(keyword_table, kw_atoms[KWID_MULTIPLE_OF], &kw_multiple_of);
  table_put(keyword_table, kw_atoms[KWID_EXCLUSIVE_MINIMUM], &kw_exclusive_min);
  table_put(keyword_table, kw_atoms[KWID_EXCLUSIVE_MAXIMUM], &kw_exclusive_max);
  table_put(keyword_table, kw_atoms[KWID_MIN_PROPERTIES], &kw_min_props);
  table_put(keyword_table, kw_atoms[KWID_MAX_PROPERTIES], &kw_max_props);
  table_put(keyword_table, kw_atoms[KWID_UNIQUE_ITEMS], &kw_unique_items);
  table_put(keyword_table, kw_atoms[KWID_MIN_LENGTH], &kw_min_length);
  table_put(keyword_table, kw_atoms[KWID_MAX_LENGTH], &kw_max_length);
  table_put(keyword_table, kw_atoms[KWID_MIN_ITEMS], &kw_min_items);
  table_put(keyword_table, kw_atoms[KWID_MAX_ITEMS], &kw_max_items);

  table_put(keyword_table, kw_atoms[KWID_ITEMS], &kw_items);
  table_put(keyword_table, kw_atoms[KWID_CONTAINS], &kw_contains);
  table_put(keyword_table, kw_atoms[KWID_NOT], &kw_not);
  table_put(keyword_table, kw_atoms[KWID_PROPERTY_NAMES], &kw_property_names);

  table_put(keyword_table, kw_atoms[KWID_PATTERN], &kw_pattern);
  table_put(keyword_table, kw_atoms[KWID_FORMAT], &kw_format);

  table_put(keyword_table, kw_atoms[KWID_ALL_OF], &kw_all_of);
  table_put(keyword_table, kw_atoms[KWID_ANY_OF], &kw_any_of);
  table_put(keyword_table, kw_atoms[KWID_ONE_OF], &kw_one_of);

  table_put(keyword_table, kw_atoms[KWID_IF], &kw_if);
  table_put(keyword_table, kw_atoms[KWID_THEN], &kw_then);
  table_put(keyword_table, kw_atoms[KWID_ELSE], &kw_else);

  table_put(keyword_table, kw_atoms[KWID_REQUIRED], &kw_required);
  table_put(keyword_table, kw_atoms[KWID_PROPERTIES], &kw_properties);
  table_put(keyword_table, kw_atoms[KWID_PATTERN_PROPERTIES], &kw_pattern_properties);
  table_put(keyword_table, kw_atoms[KWID_ADDITIONAL_PROPERTIES], &kw_additional_props);

  type_table = table_new(16, NULL, NULL);
  for (size_t i = 0; i < TYPE_MAPPINGS_COUNT; i++) {
    type_atoms[i] = atom_string(type_mappings[i].name);
    table_put(type_table, type_atoms[i], (void *)(uintptr_t)type_mappings[i].mask);
  }
  /*#endregion*/
}

// --- VISITOR STRUCTURES AND HELPERS ---

typedef struct {
  uint32_t *offsets;
  uint32_t current_offset;
  uint32_t *data_size;
  uint32_t total_size;
} SizeVisitorCtx;

typedef struct {
  const uint32_t *offsets;
  uint8_t *bytecode;
  uint32_t *code_write_ptr;
  uint32_t *data_write_ptr;
  uint32_t constant_pool_start;
} SerializeVisitorCtx;

typedef void (*SubschemaVisitor)(ASTNode *nodes, int sub_idx, void *ctx);

static inline bool is_valid_subschema_node(const ASTNode *node) {
  /*#region*/
  return node->type == AST_OBJECT || is_ast_true(node) || is_ast_false(node);
  /*#endregion*/
}

static uint32_t calculate_schema_size(ASTNode *nodes, int node_idx, uint32_t *offsets, uint32_t current_offset, uint32_t *data_size);

static void serialize_schema_direct(
    ASTNode *nodes,
    int node_idx,
    const uint32_t *offsets,
    uint8_t *bytecode,
    uint32_t *code_write_ptr,
    uint32_t *data_write_ptr,
    uint32_t constant_pool_start
);

static void size_visitor(ASTNode *nodes, int sub_idx, void *ctx) {
  /*#region*/
  SizeVisitorCtx *c = (SizeVisitorCtx *)ctx;
  uint32_t sub_size = calculate_schema_size(nodes, sub_idx, c->offsets, c->current_offset + c->total_size, c->data_size);
  c->total_size += sub_size;
  /*#endregion*/
}

static void serialize_visitor(ASTNode *nodes, int sub_idx, void *ctx) {
  /*#region*/
  SerializeVisitorCtx *c = (SerializeVisitorCtx *)ctx;
  serialize_schema_direct(nodes, sub_idx, c->offsets, c->bytecode, c->code_write_ptr, c->data_write_ptr, c->constant_pool_start);
  /*#endregion*/
}

static void visit_subschemas(ASTNode *nodes, int node_idx, SubschemaVisitor visitor, void *ctx) {
  /*#region*/
  int key_idx = nodes[node_idx].first_child;
  while (key_idx != -1) {
    ASTNode *key = &nodes[key_idx];
    ASTNode *val = &nodes[key_idx + 1];

    uint32_t len = 0;
    const char *stripped = get_stripped_string(key->token, &len);
    if (stripped) {
      const char *atom_k = atom_new(stripped, (int)len);
      KeywordConfig *cfg = (KeywordConfig *)table_get(keyword_table, atom_k);
      if (cfg) {
        switch (cfg->id) {
          case KWID_ITEMS:
          case KWID_CONTAINS:
          case KWID_NOT:
          case KWID_PROPERTY_NAMES:
          case KWID_IF:
            if (is_valid_subschema_node(val)) {
              visitor(nodes, key_idx + 1, ctx);
            }
            break;
          case KWID_ALL_OF:
          case KWID_ANY_OF:
          case KWID_ONE_OF:
            if (val->type == AST_ARRAY) {
              int sub_idx = val->first_child;
              while (sub_idx != -1) {
                if (is_valid_subschema_node(&nodes[sub_idx])) {
                  visitor(nodes, sub_idx, ctx);
                }
                sub_idx = nodes[sub_idx].next_sibling;
              }
            }
            break;
          case KWID_THEN:
          case KWID_ELSE:
            if (object_find_key_val_idx(nodes, node_idx, "if") != -1) {
              if (is_valid_subschema_node(val)) {
                visitor(nodes, key_idx + 1, ctx);
              }
            }
            break;
          case KWID_PROPERTIES:
          case KWID_PATTERN_PROPERTIES:
            if (val->type == AST_OBJECT) {
              int p_idx = val->first_child;
              while (p_idx != -1) {
                ASTNode *p_val = &nodes[p_idx + 1];
                visitor(nodes, p_idx + 1, ctx);
                p_idx = p_val->next_sibling;
              }
            }
            break;
          case KWID_ADDITIONAL_PROPERTIES:
            if (val->type == AST_OBJECT || is_ast_true(val)) {
              visitor(nodes, key_idx + 1, ctx);
            }
            break;
          default:
            break;
        }
      }
    }
    key_idx = val->next_sibling;
  }
  /*#endregion*/
}

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

  int prop_count = 0;
  int pattern_prop_count = 0;
  bool has_additional_props = false;

  // Iterate over properties of this schema object
  int key_idx = nodes[node_idx].first_child;
  while (key_idx != -1) {
    ASTNode *key = &nodes[key_idx];
    ASTNode *val = &nodes[key_idx + 1];

    uint32_t len = 0;
    const char *stripped = get_stripped_string(key->token, &len);
    if (stripped) {
      const char *atom_k = atom_new(stripped, (int)len);
      KeywordConfig *cfg = (KeywordConfig *)table_get(keyword_table, atom_k);
      if (cfg) {
        switch (cfg->id) {
          case KWID_TYPE:
          case KWID_MINIMUM:
          case KWID_MAXIMUM:
          case KWID_MULTIPLE_OF:
          case KWID_EXCLUSIVE_MINIMUM:
          case KWID_EXCLUSIVE_MAXIMUM:
          case KWID_MIN_PROPERTIES:
          case KWID_MAX_PROPERTIES:
          case KWID_MIN_LENGTH:
          case KWID_MAX_LENGTH:
          case KWID_MIN_ITEMS:
          case KWID_MAX_ITEMS:
            own_size += cfg->base_size;
            break;
          case KWID_UNIQUE_ITEMS:
            if (is_ast_true(val)) {
              own_size += cfg->base_size;
            }
            break;
          case KWID_ITEMS:
          case KWID_CONTAINS:
          case KWID_NOT:
          case KWID_PROPERTY_NAMES:
          case KWID_IF:
            if (is_valid_subschema_node(val)) {
              own_size += cfg->base_size;
            }
            break;
          case KWID_PATTERN:
          case KWID_FORMAT: {
            own_size += cfg->base_size;
            uint32_t str_len = 0;
            get_stripped_string(val->token, &str_len);
            *data_size += str_len;
            break;
          }
          case KWID_ALL_OF:
          case KWID_ANY_OF:
          case KWID_ONE_OF: {
            if (val->type == AST_ARRAY) {
              int sub_count = 0;
              int sub_idx = val->first_child;
              while (sub_idx != -1) {
                sub_count++;
                sub_idx = nodes[sub_idx].next_sibling;
              }
              own_size += cfg->base_size + sub_count * sizeof(uint32_t);
            }
            break;
          }
          case KWID_REQUIRED: {
            if (val->type == AST_ARRAY) {
              int req_count = 0;
              int r_idx = val->first_child;
              while (r_idx != -1) {
                req_count++;
                uint32_t str_len = 0;
                get_stripped_string(nodes[r_idx].token, &str_len);
                *data_size += str_len;
                r_idx = nodes[r_idx].next_sibling;
              }
              own_size += cfg->base_size + req_count * (sizeof(uint32_t) + sizeof(uint32_t));
            }
            break;
          }
          case KWID_PROPERTIES: {
            if (val->type == AST_OBJECT) {
              int p_idx = val->first_child;
              while (p_idx != -1) {
                prop_count++;
                uint32_t str_len = 0;
                get_stripped_string(nodes[p_idx].token, &str_len);
                *data_size += str_len;
                p_idx = nodes[p_idx + 1].next_sibling;
              }
            }
            break;
          }
          case KWID_PATTERN_PROPERTIES: {
            if (val->type == AST_OBJECT) {
              int p_idx = val->first_child;
              while (p_idx != -1) {
                pattern_prop_count++;
                uint32_t str_len = 0;
                get_stripped_string(nodes[p_idx].token, &str_len);
                *data_size += str_len;
                p_idx = nodes[p_idx + 1].next_sibling;
              }
            }
            break;
          }
          case KWID_ADDITIONAL_PROPERTIES:
            has_additional_props = true;
            break;
          default:
            break;
        }
      }
    }
    key_idx = val->next_sibling;
  }

  if (prop_count > 0 || pattern_prop_count > 0 || has_additional_props) {
    own_size += 1 + sizeof(uint32_t) + sizeof(uint32_t) + sizeof(int32_t)
             + prop_count * (sizeof(uint32_t) * 3)
             + pattern_prop_count * (sizeof(uint32_t) * 3);
  }
  own_size += 1; // OP_END

  uint32_t total_size = own_size;

  SizeVisitorCtx visitor_ctx;
  visitor_ctx.offsets = offsets;
  visitor_ctx.current_offset = current_offset;
  visitor_ctx.data_size = data_size;
  visitor_ctx.total_size = total_size;

  visit_subschemas(nodes, node_idx, size_visitor, &visitor_ctx);

  return visitor_ctx.total_size;
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

    uint32_t len = 0;
    const char *stripped = get_stripped_string(key->token, &len);
    if (stripped) {
      const char *atom_k = atom_new(stripped, (int)len);
      KeywordConfig *cfg = (KeywordConfig *)table_get(keyword_table, atom_k);
      if (cfg) {
        switch (cfg->id) {
          case KWID_TYPE: {
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
            break;
          }
          case KWID_MINIMUM:
            has_min = true;
            min_val = parse_number(val->token);
            break;
          case KWID_MAXIMUM:
            has_max = true;
            max_val = parse_number(val->token);
            break;
          case KWID_MULTIPLE_OF:
            has_multiple_of = true;
            multiple_of_val = parse_number(val->token);
            break;
          case KWID_EXCLUSIVE_MINIMUM:
            has_ex_min = true;
            ex_min_val = parse_number(val->token);
            break;
          case KWID_EXCLUSIVE_MAXIMUM:
            has_ex_max = true;
            ex_max_val = parse_number(val->token);
            break;
          case KWID_PATTERN:
            has_pattern = true;
            pattern_token = val->token;
            break;
          case KWID_MIN_PROPERTIES:
            has_min_props = true;
            min_props = (int32_t)parse_number(val->token);
            break;
          case KWID_MAX_PROPERTIES:
            has_max_props = true;
            max_props = (int32_t)parse_number(val->token);
            break;
          case KWID_UNIQUE_ITEMS:
            has_unique_items = is_ast_true(val);
            break;
          case KWID_MIN_LENGTH:
            has_min_len = true;
            min_len = (int32_t)parse_number(val->token);
            break;
          case KWID_MAX_LENGTH:
            has_max_len = true;
            max_len = (int32_t)parse_number(val->token);
            break;
          case KWID_MIN_ITEMS:
            has_min_items = true;
            min_items = (int32_t)parse_number(val->token);
            break;
          case KWID_MAX_ITEMS:
            has_max_items = true;
            max_items = (int32_t)parse_number(val->token);
            break;
          case KWID_ITEMS:
            if (is_valid_subschema_node(val)) {
              has_items = true;
              items_val_idx = key_idx + 1;
            }
            break;
          case KWID_CONTAINS:
            if (is_valid_subschema_node(val)) {
              has_contains = true;
              contains_val_idx = key_idx + 1;
            }
            break;
          case KWID_NOT:
            if (is_valid_subschema_node(val)) {
              has_not = true;
              not_val_idx = key_idx + 1;
            }
            break;
          case KWID_ALL_OF:
            if (val->type == AST_ARRAY) {
              has_all_of = true;
              all_of_val_idx = key_idx + 1;
              int sub_idx = val->first_child;
              while (sub_idx != -1) {
                all_of_count++;
                sub_idx = nodes[sub_idx].next_sibling;
              }
            }
            break;
          case KWID_ANY_OF:
            if (val->type == AST_ARRAY) {
              has_any_of = true;
              any_of_val_idx = key_idx + 1;
              int sub_idx = val->first_child;
              while (sub_idx != -1) {
                any_of_count++;
                sub_idx = nodes[sub_idx].next_sibling;
              }
            }
            break;
          case KWID_ONE_OF:
            if (val->type == AST_ARRAY) {
              has_one_of = true;
              one_of_val_idx = key_idx + 1;
              int sub_idx = val->first_child;
              while (sub_idx != -1) {
                one_of_count++;
                sub_idx = nodes[sub_idx].next_sibling;
              }
            }
            break;
          case KWID_IF:
            if (is_valid_subschema_node(val)) {
              has_if = true;
              if_val_idx = key_idx + 1;
            }
            break;
          case KWID_PROPERTY_NAMES:
            if (is_valid_subschema_node(val)) {
              has_property_names = true;
              property_names_val_idx = key_idx + 1;
            }
            break;
          case KWID_FORMAT:
            has_format = true;
            format_token = val->token;
            break;
          case KWID_THEN:
            if (is_valid_subschema_node(val)) {
              then_val_idx = key_idx + 1;
            }
            break;
          case KWID_ELSE:
            if (is_valid_subschema_node(val)) {
              else_val_idx = key_idx + 1;
            }
            break;
          case KWID_REQUIRED:
            if (val->type == AST_ARRAY) {
              required_val_idx = key_idx + 1;
              int r_idx = val->first_child;
              while (r_idx != -1) {
                required_count++;
                r_idx = nodes[r_idx].next_sibling;
              }
            }
            break;
          case KWID_PROPERTIES:
            if (val->type == AST_OBJECT) {
              properties_val_idx = key_idx + 1;
              int p_idx = val->first_child;
              while (p_idx != -1) {
                prop_count++;
                p_idx = nodes[p_idx + 1].next_sibling;
              }
            }
            break;
          case KWID_PATTERN_PROPERTIES:
            if (val->type == AST_OBJECT) {
              pattern_properties_val_idx = key_idx + 1;
              int p_idx = val->first_child;
              while (p_idx != -1) {
                pattern_prop_count++;
                p_idx = nodes[p_idx + 1].next_sibling;
              }
            }
            break;
          case KWID_ADDITIONAL_PROPERTIES:
            has_additional_props = true;
            additional_props_val_idx = key_idx + 1;
            break;
          default:
            break;
        }
      }
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

  SerializeVisitorCtx visitor_ctx;
  visitor_ctx.offsets = offsets;
  visitor_ctx.bytecode = bytecode;
  visitor_ctx.code_write_ptr = code_write_ptr;
  visitor_ctx.data_write_ptr = data_write_ptr;
  visitor_ctx.constant_pool_start = constant_pool_start;

  visit_subschemas(nodes, node_idx, serialize_visitor, &visitor_ctx);
  /*#endregion*/
}

// --- PUBLIC API ---

uint8_t *compile_schema(Jsonv_Arena *arena, ASTNode *nodes, int ast_count, int root_idx, int *out_length) {
  /*#region*/
  init_keyword_table();

  if (ast_count <= 0) {
    *out_length = 0;
    return NULL;
  }

  // No need to handle null pointer, error is automatically raised
  uint32_t *offsets = (uint32_t *)arena_alloc(arena, ast_count * sizeof(uint32_t));

  for (int i = 0; i < ast_count; i++) {
    offsets[i] = (uint32_t)-1;
  }

  uint32_t data_size = 0;
  uint32_t code_size = calculate_schema_size(nodes, root_idx, offsets, sizeof(BytecodeHeader), &data_size);
  uint32_t total_size = sizeof(BytecodeHeader) + code_size + data_size;

  // No need to handle null pointer, error is automatically raised
  uint8_t *bytecode = (uint8_t *)arena_alloc(arena, total_size);

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

extern const Except MALFORMED_JSON;

bool is_compiled_schema_match(
    Jsonv_Arena *arena,
    const char *schema_json,
    size_t json_len,
    const uint8_t *bytecode,
    size_t bytecode_len
) {
  /*#region*/
  if (!schema_json || !bytecode || json_len == 0) {
    return false;
  }

  Lexer lexer;
  lexer_init(&lexer, (const unsigned char *)schema_json, json_len);

  Stack ast = {0};
  set_t schema_set;
  KeyTreePool schema_keytree;

  bool result = false;

  TRY {
    JsonEstimate est = {0};
    prescan(schema_json, json_len, &est);

    parse_to_ast(arena, &lexer, json_len, est.value_count, &ast, &schema_keytree, &schema_set);

    int compiled_len = 0;
    uint8_t *compiled = compile_schema(arena, (ASTNode *)ast.data, stack_size(&ast), 0, &compiled_len);

    if (compiled && (size_t)compiled_len == bytecode_len) {
      result = (memcmp(compiled, bytecode, bytecode_len) == 0);
    }
  }
  ELSE {
    result = false;
  }
  END_TRY;

  return result;
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
    printf("%.*s", (int)t.value.raw_number.length, (const char *)t.value.raw_number.start);
  } else if (t.type == T_TRUE) {
    printf("true");
  } else if (t.type == T_FALSE) {
    printf("false");
  } else if (t.type == T_NULL) {
    printf("null");
  }
  /*#endregion*/
}

void jsonv_schema_clear_static_tables(void) {
  /*#region*/
  if(!keyword_table)return;
  keyword_table = NULL;
  type_table = NULL;
  for (int i = 0; i < KWID_COUNT; i++) {
    kw_atoms[i] = NULL;
  }
  for (size_t i = 0; i < TYPE_MAPPINGS_COUNT; i++) {
    type_atoms[i] = NULL;
  }
  atom_clear();
  /*#endregion*/
}

