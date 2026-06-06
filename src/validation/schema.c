#include "schema.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// --- SCHEMA COMPILER STRUCTURES ---

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

typedef struct SchemaRule {
  // Core Validation
  int type_mask; // Allowed types bitmask
  int next_rule; // Chain index (for logical grouping if needed)

  // Numbers
  double min;
  double max;
  bool has_min;
  bool has_max;

  // Strings
  int min_len;
  int max_len;

  // Arrays
  int items_rule; // Rule for all items
  int min_items;
  int max_items;

  // Objects
  PropertyRule *props;
  int prop_count;
  Token *required;
  int required_count;
  int additional_props_rule; // -1 if allowed, -2 if disallowed, >=0 is a rule
                             // index
} SchemaRule;

// Task definition for the stack
typedef enum {
  PATCH_NONE,
  PATCH_ITEMS,
  PATCH_PROPERTY,
  PATCH_ADDITIONAL_PROPS
} PatchType;

typedef struct {
  int ast_idx;     // Current JSON AST node to compile
  int parent_rule; // The rule that spawned this task
  PatchType patch; // How to link back to parent
  int patch_idx;   // Index in parent's array (e.g., props[i])
} CompileTask;

// --- HELPERS ---

static inline bool is_ast_string(const ASTNode *n) {
  return n->type == AST_STRING || (n->type == AST_LEAF && n->token.type == T_STRING);
}

static inline bool is_ast_true(const ASTNode *n) {
  return n->type == AST_TRUE || (n->type == AST_LEAF && n->token.type == T_TRUE);
}

static inline bool is_ast_false(const ASTNode *n) {
  return n->type == AST_FALSE || (n->type == AST_LEAF && n->token.type == T_FALSE);
}


bool token_equals(Token t, const char *str) {
  /*#region*/
  if (t.type != T_STRING)
    return false;

  const char *t_start = (const char *)t.value.string.start;
  size_t t_len = t.value.string.length;

  // Check if the token is wrapped in quotes (e.g., "type" vs type)
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
  // Fallback if number is stored as string
  if (t.type == T_STRING) {
    // rudimentary parse, ideally use strtod on temp buffer
    return 0.0;
  }
  return 0.0;
  /*#endregion*/
}

// Allocates a new rule slot from our pre-allocated Arena pool
static int emit_rule(SchemaRule *rules, int *count, int cap) {
  /*#region*/
  if (*count >= cap) {
    return -1;
  }

  int idx = (*count)++;
  SchemaRule *r = &rules[idx];

  // Initialize defaults
  memset(r, 0, sizeof(SchemaRule));
  r->type_mask = 0; // 0 means "any type allowed" by default in JSON Schema
                    // unless specified
  r->next_rule = -1;
  r->items_rule = -1;
  r->additional_props_rule =
      -1; // Default: allowed (logic: -1=allow, -2=false, >=0=rule)
  r->min_len = -1;
  r->max_len = -1;
  r->min_items = -1;
  r->max_items = -1;

  return idx;
  /*#endregion*/
}

int map_type_string_to_mask(Token t) {
  /*#region*/
  if (token_equals(t, "string"))
    return TYPE_STRING;
  if (token_equals(t, "number"))
    return TYPE_NUMBER;
  if (token_equals(t, "integer"))
    return TYPE_NUMBER | TYPE_INTEGER;
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

// --- MAIN COMPILER LOGIC ---

static SchemaRule *compile_schema_to_rules(Jsonv_Arena *arena, ASTNode *nodes, int ast_count, int root_idx, int *out_count) {
  /*#region*/
  if (ast_count <= 0) return NULL;
  
  // Pre-allocate rules array on the Arena based on total AST count
  SchemaRule *rules = (SchemaRule *)jsonv_arena_alloc(arena, ast_count * sizeof(SchemaRule));
  if (!rules) return NULL;
  
  int rule_count = 0;

  // Stack setup
  unsigned char stack_buf[4096]; // 4KB stack buffer
  Stack stack;
  stack_init(&stack, sizeof(CompileTask), stack_buf, sizeof(stack_buf));

  // Initial Task
  CompileTask root_task = {root_idx, -1, PATCH_NONE, 0};
  stack_push(&stack, &root_task);

  while (!stack_is_empty(&stack)) {
    CompileTask task = *(CompileTask *)stack_pop(&stack);
    ASTNode *node = &nodes[task.ast_idx];

    // 1. Create Rule
    int curr_idx = emit_rule(rules, &rule_count, ast_count);
    if (curr_idx == -1) {
      fprintf(stderr, "Memory allocation failed\n");
      stack_destroy(&stack);
      return NULL;
    }

    // 2. Patch Parent
    if (task.parent_rule != -1) {
      SchemaRule *parent = &rules[task.parent_rule];
      if (task.patch == PATCH_ITEMS) {
        parent->items_rule = curr_idx;
      } else if (task.patch == PATCH_PROPERTY) {
        parent->props[task.patch_idx].rule_index = curr_idx;
      } else if (task.patch == PATCH_ADDITIONAL_PROPS) {
        parent->additional_props_rule = curr_idx;
      }
    }

    // 3. Process Schema Keywords
    // If the AST node is simple boolean true/false schema
    if (is_ast_true(node)) {
      // empty rule = allow everything
      continue;
    }
    if (is_ast_false(node)) {
      // "type_mask = -1" or specific flag to indicate "fail always"
      // For now, let's set type_mask to something impossible or flag it
      rules[curr_idx].type_mask = -1; // -1 could signal "fail"
      continue;
    }
    if (node->type != AST_OBJECT)
      continue;

    // Iterate over schema keywords
    int key_idx = node->first_child;
    while (key_idx != -1) {
      ASTNode *key = &nodes[key_idx];
      ASTNode *val = &nodes[key_idx + 1]; // Value is next sibling

      // KEYWORD: "type"
      if (token_equals(key->token, "type")) {
        if (is_ast_string(val)) {
          rules[curr_idx].type_mask = map_type_string_to_mask(val->token);
        } else if (val->type == AST_ARRAY) {
          // Handle ["string", "null"]
          int t_idx = val->first_child;
          int mask = 0;
          while (t_idx != -1) {
            mask |= map_type_string_to_mask(nodes[t_idx].token);
            t_idx = nodes[t_idx].next_sibling;
          }
          rules[curr_idx].type_mask = mask;
        }
      }

      // KEYWORDS: Numbers
      else if (token_equals(key->token, "minimum")) {
        rules[curr_idx].has_min = true;
        rules[curr_idx].min = parse_number(val->token);
      } else if (token_equals(key->token, "maximum")) {
        rules[curr_idx].has_max = true;
        rules[curr_idx].max = parse_number(val->token);
      }

      // KEYWORDS: Strings
      else if (token_equals(key->token, "minLength")) {
        rules[curr_idx].min_len = (int)parse_number(val->token);
      } else if (token_equals(key->token, "maxLength")) {
        rules[curr_idx].max_len = (int)parse_number(val->token);
      }

      // KEYWORDS: Arrays
      else if (token_equals(key->token, "items")) {
        if (val->type == AST_OBJECT || is_ast_true(val) ||
            is_ast_false(val)) {
          CompileTask t = {key_idx + 1, curr_idx, PATCH_ITEMS, 0};
          stack_push(&stack, &t);
        }
      }

      // KEYWORDS: Objects
      else if (token_equals(key->token, "properties") &&
               val->type == AST_OBJECT) {
        // First pass: Count properties to allocate memory
        int count = 0;
        int p_idx = val->first_child;
        while (p_idx != -1) {
          count++;
          p_idx = nodes[p_idx + 1].next_sibling; // Skip key+val
        }

        // Allocate property rules on the Arena
        rules[curr_idx].props = (PropertyRule *)jsonv_arena_alloc(arena, count * sizeof(PropertyRule));
        if (count > 0 && !rules[curr_idx].props) {
          stack_destroy(&stack);
          return NULL;
        }
        rules[curr_idx].prop_count = count;

        // Second pass: Create Tasks
        p_idx = val->first_child;
        int slot = 0;
        while (p_idx != -1) {
          ASTNode *p_key = &nodes[p_idx];

          // Fill key now
          rules[curr_idx].props[slot].key = p_key->token;
          rules[curr_idx].props[slot].rule_index = -1;

          // Schedule value compilation
          CompileTask t = {p_idx + 1, curr_idx, PATCH_PROPERTY, slot};
          stack_push(&stack, &t);

          slot++;
          p_idx = nodes[p_idx + 1].next_sibling;
        }
      } else if (token_equals(key->token, "required") &&
                 val->type == AST_ARRAY) {
        int count = 0;
        int r_idx = val->first_child;
        // Count
        while (r_idx != -1) {
          count++;
          r_idx = nodes[r_idx].next_sibling;
        }

        // Allocate required list on the Arena
        rules[curr_idx].required = (Token *)jsonv_arena_alloc(arena, count * sizeof(Token));
        if (count > 0 && !rules[curr_idx].required) {
          stack_destroy(&stack);
          return NULL;
        }
        rules[curr_idx].required_count = count;

        // Fill
        r_idx = val->first_child;
        int slot = 0;
        while (r_idx != -1) {
          rules[curr_idx].required[slot++] = nodes[r_idx].token;
          r_idx = nodes[r_idx].next_sibling;
        }
      } else if (token_equals(key->token, "additionalProperties")) {
        if (is_ast_false(val)) {
          rules[curr_idx].additional_props_rule = -2; // Disallowed
        } else if (val->type == AST_OBJECT || is_ast_true(val)) {
          CompileTask t = {key_idx + 1, curr_idx, PATCH_ADDITIONAL_PROPS, 0};
          stack_push(&stack, &t);
        }
      }

      key_idx = val->next_sibling;
    }
  }

  stack_destroy(&stack);
  *out_count = rule_count;
  return rules;
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

static uint32_t get_rule_serialized_size(const SchemaRule *r) {
  /*#region*/
  uint32_t size = 0;
  if (r->type_mask == -1) {
    size += 1; // OP_FAIL
    return size;
  }
  if (r->type_mask > 0) {
    size += 1 + sizeof(uint32_t); // OP_TYPE + mask
  }
  if (r->has_min) {
    size += 1 + sizeof(double); // OP_MINIMUM + min
  }
  if (r->has_max) {
    size += 1 + sizeof(double); // OP_MAXIMUM + max
  }
  if (r->min_len >= 0) {
    size += 1 + sizeof(int32_t); // OP_MIN_LENGTH + min_len
  }
  if (r->max_len >= 0) {
    size += 1 + sizeof(int32_t); // OP_MAX_LENGTH + max_len
  }
  if (r->min_items >= 0) {
    size += 1 + sizeof(int32_t); // OP_MIN_ITEMS + min_items
  }
  if (r->max_items >= 0) {
    size += 1 + sizeof(int32_t); // OP_MAX_ITEMS + max_items
  }
  if (r->items_rule >= 0) {
    size += 1 + sizeof(uint32_t); // OP_ITEMS + offset
  }
  if (r->required_count > 0) {
    size += 1 + sizeof(uint32_t) + r->required_count * sizeof(Token); // OP_REQUIRED + count + tokens
  }
  if (r->prop_count > 0 || r->additional_props_rule != -1) {
    size += 1 + sizeof(uint32_t) + sizeof(int32_t) + r->prop_count * (sizeof(Token) + sizeof(uint32_t)); // OP_PROPERTIES + count + add_rule + key/offset pairs
  }
  size += 1; // OP_END
  return size;
  /*#endregion*/
}

static uint8_t *serialize_schema(Jsonv_Arena *arena, const SchemaRule *rules, int rule_count, int *out_length) {
  /*#region*/
  if (rule_count <= 0) {
    *out_length = 0;
    return NULL;
  }

  // Step 1: Compute starting offset of each rule
  uint32_t *offsets = (uint32_t *)jsonv_arena_alloc(arena, rule_count * sizeof(uint32_t));
  if (!offsets) return NULL;

  uint32_t total_size = 0;
  for (int i = 0; i < rule_count; i++) {
    offsets[i] = total_size;
    total_size += get_rule_serialized_size(&rules[i]);
  }

  // Step 2: Allocate the compact contiguous bytecode array
  uint8_t *bytecode = (uint8_t *)jsonv_arena_alloc(arena, total_size);
  if (!bytecode) return NULL;

  uint8_t *pc = bytecode;

  // Step 3: Serialize each rule
  for (int i = 0; i < rule_count; i++) {
    const SchemaRule *r = &rules[i];

    if (r->type_mask == -1) {
      emit_byte(&pc, OP_FAIL);
      continue;
    }

    if (r->type_mask > 0) {
      emit_byte(&pc, OP_TYPE);
      emit_uint32(&pc, (uint32_t)r->type_mask);
    }

    if (r->has_min) {
      emit_byte(&pc, OP_MINIMUM);
      emit_double(&pc, r->min);
    }

    if (r->has_max) {
      emit_byte(&pc, OP_MAXIMUM);
      emit_double(&pc, r->max);
    }

    if (r->min_len >= 0) {
      emit_byte(&pc, OP_MIN_LENGTH);
      emit_int32(&pc, (int32_t)r->min_len);
    }

    if (r->max_len >= 0) {
      emit_byte(&pc, OP_MAX_LENGTH);
      emit_int32(&pc, (int32_t)r->max_len);
    }

    if (r->min_items >= 0) {
      emit_byte(&pc, OP_MIN_ITEMS);
      emit_int32(&pc, (int32_t)r->min_items);
    }

    if (r->max_items >= 0) {
      emit_byte(&pc, OP_MAX_ITEMS);
      emit_int32(&pc, (int32_t)r->max_items);
    }

    if (r->items_rule >= 0) {
      emit_byte(&pc, OP_ITEMS);
      emit_uint32(&pc, offsets[r->items_rule]);
    }

    if (r->required_count > 0) {
      emit_byte(&pc, OP_REQUIRED);
      emit_uint32(&pc, (uint32_t)r->required_count);
      for (int k = 0; k < r->required_count; k++) {
        emit_token(&pc, r->required[k]);
      }
    }

    if (r->prop_count > 0 || r->additional_props_rule != -1) {
      emit_byte(&pc, OP_PROPERTIES);
      emit_uint32(&pc, (uint32_t)r->prop_count);
      int32_t add_rule = r->additional_props_rule;
      if (add_rule >= 0) {
        add_rule = (int32_t)offsets[add_rule];
      }
      emit_int32(&pc, add_rule);

      for (int k = 0; k < r->prop_count; k++) {
        emit_token(&pc, r->props[k].key);
        uint32_t target_offset = 0;
        if (r->props[k].rule_index >= 0) {
          target_offset = offsets[r->props[k].rule_index];
        }
        emit_uint32(&pc, target_offset);
      }
    }

    emit_byte(&pc, OP_END);
  }

  *out_length = (int)total_size;
  return bytecode;
  /*#endregion*/
}

uint8_t *compile_schema(Jsonv_Arena *arena, ASTNode *nodes, int ast_count, int root_idx, int *out_length) {
  /*#region*/
  int rule_count = 0;
  SchemaRule *rules = compile_schema_to_rules(arena, nodes, ast_count, root_idx, &rule_count);
  if (!rules) {
    return NULL;
  }
  return serialize_schema(arena, rules, rule_count, out_length);
  /*#endregion*/
}

// --- DEBUGGING ---

void print_token(Token t) {
  /*#region*/
  if (t.type == T_STRING) {
    const char *start = (const char *)t.value.string.start;
    size_t len = t.value.string.length;
    // Avoid double quoting if the token itself contains quotes
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

