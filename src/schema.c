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

// Resizes the rule array and returns the index of the new slot
int emit_rule(SchemaRule **rules, int *count, int *cap) {
  /*#region*/
  if (*count >= *cap) {
    *cap = (*cap == 0) ? 64 : *cap * 2;
    SchemaRule *new_ptr = realloc(*rules, *cap * sizeof(SchemaRule));
    if (!new_ptr)
      return -1;
    *rules = new_ptr;
  }

  int idx = (*count)++;
  SchemaRule *r = &(*rules)[idx];

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

SchemaRule *compile_schema(ASTNode *nodes, int root_idx, int *out_count) {
  /*#region*/
  int rule_cap = 0;
  int rule_count = 0;
  SchemaRule *rules = NULL;

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
    int curr_idx = emit_rule(&rules, &rule_count, &rule_cap);
    if (curr_idx == -1) {
      fprintf(stderr, "Memory allocation failed\n");
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
    if (node->type == AST_TRUE) {
      // empty rule = allow everything
      continue;
    }
    if (node->type == AST_FALSE) {
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
        if (val->type == AST_STRING) {
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
        if (val->type == AST_OBJECT || val->type == AST_TRUE ||
            val->type == AST_FALSE) {
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

        // Allocate
        rules[curr_idx].props = malloc(count * sizeof(PropertyRule));
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

        rules[curr_idx].required = malloc(count * sizeof(Token));
        rules[curr_idx].required_count = count;

        // Fill
        r_idx = val->first_child;
        int slot = 0;
        while (r_idx != -1) {
          rules[curr_idx].required[slot++] = nodes[r_idx].token;
          r_idx = nodes[r_idx].next_sibling;
        }
      } else if (token_equals(key->token, "additionalProperties")) {
        if (val->type == AST_FALSE) {
          rules[curr_idx].additional_props_rule = -2; // Disallowed
        } else if (val->type == AST_OBJECT || val->type == AST_TRUE) {
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

void print_schema_rules(SchemaRule *rules, int count) {
  /*#region*/
  printf("=== COMPILED SCHEMA (%d rules) ===\n", count);
  for (int i = 0; i < count; i++) {
    SchemaRule *r = &rules[i];
    printf("RULE [%d]:\n", i);

    if (r->type_mask) {
      printf("  Type Mask: 0x%X ( ", r->type_mask);
      if (r->type_mask & TYPE_STRING)
        printf("String ");
      if (r->type_mask & TYPE_NUMBER)
        printf("Number ");
      if (r->type_mask & TYPE_OBJECT)
        printf("Object ");
      if (r->type_mask & TYPE_ARRAY)
        printf("Array ");
      if (r->type_mask & TYPE_BOOL)
        printf("Bool ");
      if (r->type_mask & TYPE_NULL)
        printf("Null ");
      printf(")\n");
    }

    if (r->has_min)
      printf("  Min: %g\n", r->min);
    if (r->has_max)
      printf("  Max: %g\n", r->max);
    if (r->min_len != -1)
      printf("  MinLen: %d\n", r->min_len);
    if (r->max_len != -1)
      printf("  MaxLen: %d\n", r->max_len);

    if (r->items_rule != -1)
      printf("  Items -> Rule %d\n", r->items_rule);

    if (r->prop_count > 0) {
      printf("  Properties (%d):\n", r->prop_count);
      for (int k = 0; k < r->prop_count; k++) {
        printf("    - ");
        print_token(r->props[k].key);
        printf(": -> Rule %d\n", r->props[k].rule_index);
      }
    }

    if (r->required_count > 0) {
      printf("  Required: [");
      for (int k = 0; k < r->required_count; k++) {
        print_token(r->required[k]);
        printf(k < r->required_count - 1 ? ", " : "");
      }
      printf("]\n");
    }

    if (r->additional_props_rule == -2)
      printf("  AddlProps: DISALLOWED\n");
    else if (r->additional_props_rule >= 0)
      printf("  AddlProps: -> Rule %d\n", r->additional_props_rule);

    printf("\n");
  }
  printf("==================================\n");
  /*#endregion*/
}
