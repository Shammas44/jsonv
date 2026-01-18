#include "validate.h"
#include "assert.h"
#include "ast.h"
#include "schema.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ERROR(format, ...)                                                     \
  char *p = jsonv_build_data_path(data, json_data);                            \
  jsonv_path_reset(path);                                                      \
  JSONV_ENTER_FIELD(path, p);                                                  \
  JSONV_ERR(errors, path, format, __VA_ARGS__);                                \
  JSONV_LEAVE(path);

#define VALIDATION_ERROR(t, fmt, ...)                                          \
  do {                                                                         \
    (*(error))->type = t;                                                      \
    snprintf((*(error))->description, 100, "Validation Error: " fmt,           \
             __VA_ARGS__);                                                     \
  } while (0)

// Macros for safe access
#define SCH_NODE(s, i) (&((SchemaNode *)(s)->data)[i])
#define JSON_NODE(s, i) (&((ASTNode *)(s)->data)[i])

// Helper: Parse double from Token (for numeric comparisons)
static double parse_number_token(Token t) { return t.number; }

static bool token_equals(Token a, Token b) {
  /*#region*/
  // Simple equality check
  // If one is quoted and one isn't, you might need normalization logic here.
  // For now, assuming both come from similar sources or are normalized.
  if (a.string.length != b.string.length)
    return false;
  return strncmp((char *)a.string.start, (char *)b.string.start,
                 a.string.length) == 0;
  /*#endregion*/
}

static int check_constraints(int d_node_idx, SchemaNode *s_node,
                             Stack *schema_stack, Stack *data_stack,
                             E **error) {
  /*#region*/
  ASTNode *d_node = JSON_NODE_FROM_STACK(data_stack, d_node_idx);

  for (int i = 0; i < s_node->constraints_count; i++) {
    Constraint *c = &s_node->constraints[i];

    if (strncmp((char *)c->name.start, "additionalProperties",
                c->name.length) == 0) {
      if (c->type == T_FALSE && d_node->type == AST_OBJECT) {
        int curr = d_node->first_child;
        while (curr != -1) {
          Token key = JSON_NODE_FROM_STACK(data_stack, curr)->token;
          bool allowed = false;
          int s_curr = s_node->props_head;
          while (s_curr != -1) {
            if (token_equals(key,
                             SCH_NODE_FROM_STACK(schema_stack, s_curr)->name)) {
              allowed = true;
              break;
            }
            s_curr = SCH_NODE_FROM_STACK(schema_stack, s_curr)->next_sibling;
          }
          if (!allowed) {
            VALIDATION_ERROR(Jsonv_AdditionalProperties_error,
                             "Additional property '%.*s' not allowed.",
                             (int)key.string.length, key.string.start);
            return curr;
          }
          int val = JSON_NODE_FROM_STACK(data_stack, curr)->next_sibling;
          curr = JSON_NODE_FROM_STACK(data_stack, val)->next_sibling;
        }
      }
    } else if (c->type == T_NUMBER) {

      if (strncmp((char *)c->name.start, "minItems", c->name.length) == 0 &&
          d_node->type == AST_ARRAY) {
        int count = 0;
        int curr = d_node->first_child;
        while (curr != -1) {
          count++;
          curr = JSON_NODE_FROM_STACK(data_stack, curr)->next_sibling;
        }
        if (count < c->value.number) {
          VALIDATION_ERROR(Jsonv_MinItems_error,
                           "Array too short (minItems %.0f).", c->value.number);
          return d_node_idx;
        }
      }

      else if (strncmp((char *)c->name.start, "minimum", c->name.length) == 0) {
        double input = d_node->token.number;
        if (input < c->value.number) {
          VALIDATION_ERROR(Jsonv_Minimum_error,
                           "Value too small. Expected >= %.2f, got %.2f",
                           c->value.number, input);
          return d_node_idx;
        }
      }

      else if (strncmp((char *)c->name.start, "maximum", c->name.length) == 0) {
        double input = parse_number_token(d_node->token);
        if (input > c->value.number) {
          VALIDATION_ERROR(Jsonv_Maximum_error,
                           "Value too large. Expected <= %.2f, got %.2f",
                           c->value.number, input);
          return d_node_idx;
        }
      }

      else if (strncmp((char *)c->name.start, "multipleOf", c->name.length) ==
               0) {
        int input = parse_number_token(d_node->token);
        if (input % (int)c->value.number != 0) {
          VALIDATION_ERROR(Jsonv_MultipleOf_error,
                           "Expected %d to be multiple of %d.", input,
                           (int)c->value.number);
          return d_node_idx;
        }
      }

      else if (strncmp((char *)c->name.start, "exclusiveMaximum",
                       c->name.length) == 0) {
        double input = parse_number_token(d_node->token);
        if (input >= c->value.number) {
          VALIDATION_ERROR(Jsonv_ExclusiveMaximum_error,
                           "Value too large. Expected < %.2f, got %.2f.",
                           c->value.number, input);
          return d_node_idx;
        }
      }

      else if (strncmp((char *)c->name.start, "exclusiveMinimum",
                       c->name.length) == 0) {
        double input = parse_number_token(d_node->token);
        if (input <= c->value.number) {
          VALIDATION_ERROR(Jsonv_ExclusiveMinimum_error,
                           "Value too small. Expected > %.2f, got %.2f.",
                           c->value.number, input);
          return d_node_idx;
        }
      }

    } else if (c->type == T_STRING) {

      if (strncmp((char *)c->name.start, "minLength", c->name.length) == 0) {
        // Note: Token length includes quotes for strings. Actual len = len - 2
        int actual_len = d_node->token.string.length >= 2
                             ? d_node->token.string.length - 2
                             : 0;
        if (actual_len < c->value.number) {
          VALIDATION_ERROR(Jsonv_MinLength_error,
                           "String too short. Expected >= %.0f, got %d.",
                           c->value.number, actual_len);
          return d_node_idx;
        }
      }

      else if (strncmp((char *)c->name.start, "maxLength", c->name.length) ==
               0) {
        int actual_len = d_node->token.string.length >= 2
                             ? d_node->token.string.length - 2
                             : 0;
        if (actual_len > c->value.number) {
          VALIDATION_ERROR(Jsonv_MaxLength_error,
                           "String too long. Expected <= %.0f, got %d.",
                           c->value.number, actual_len);
          return d_node_idx;
        }
      }
    }
  }
  return JSONV_SCHEMA_IS_VALID;
  /*#endregion*/
}

static int validate_recursive(Stack *data, int data_idx, Stack *schema,
                              int schema_idx, E **error) {
  /*#region*/
  if (data_idx == -1 || schema_idx == -1)
    return JSONV_SCHEMA_IS_VALID;

  ASTNode *d_node = JSON_NODE_FROM_STACK(data, data_idx);
  SchemaNode *s_node = SCH_NODE_FROM_STACK(schema, schema_idx);
  int error_res = -1;

  // 0. Immediate Fail
  if (s_node->type == SC_FALSE) {
    VALIDATION_ERROR(Jsonv_ValueNotAllowed_error,
                     "Value not allowed (Schema is false).", NULL);
    error_res = data_idx;
    goto end;
  }
  if (s_node->type == SC_ANY)
    goto end; // Success

  // 1. Type Match
  bool type_match = false;
  switch (s_node->type) {
  case SC_STRING:
    type_match = (d_node->token.type == T_STRING);
    break;
  case SC_NUMBER:
    type_match = (d_node->token.type == T_NUMBER);
    break;
  case SC_BOOLEAN:
    type_match =
        (d_node->token.type == T_TRUE || d_node->token.type == T_FALSE);
    break;
  case SC_NULL:
    type_match = (d_node->token.type == T_NULL);
    break;
  case SC_OBJECT:
    type_match = (d_node->type == AST_OBJECT);
    break;
  case SC_ARRAY:
    type_match = (d_node->type == AST_ARRAY);
    break;
  default:
    type_match = true;
    break;
  }

  if (!type_match) {
    VALIDATION_ERROR(Jsonv_Type_error, "Type mismatch. Expected type %d\n",
                     s_node->type);
    error_res = data_idx;
    goto end;
  }

  // 2. Constraints
  {
    int res = check_constraints(data_idx, s_node, schema, data, error);
    if (res != -1) {
      error_res = res;
      goto end;
    }
  }

  // 3. Structure
  if (s_node->type == SC_OBJECT && d_node->type == AST_OBJECT) {
    // A. Required
    int req = s_node->required_head;
    while (req != -1) {
      Token r_name = SCH_NODE_FROM_STACK(schema, req)->name;
      bool found = false;
      int child = d_node->first_child;
      while (child != -1) {
        if (token_equals(JSON_NODE_FROM_STACK(data, child)->token, r_name)) {
          found = true;
          break;
        }
        int val = JSON_NODE_FROM_STACK(data, child)->next_sibling;
        child = JSON_NODE_FROM_STACK(data, val)->next_sibling;
      }
      if (!found) {
        VALIDATION_ERROR(Jsonv_Required_error,
                         "Missing required field '%.*s'\n",
                         (int)r_name.string.length, r_name.string.start);
        error_res = data_idx;
        goto end;
      }
      req = SCH_NODE_FROM_STACK(schema, req)->next_sibling;
    }

    // B. Children
    int d_key_idx = d_node->first_child;
    while (d_key_idx != -1) {
      int d_val_idx = JSON_NODE_FROM_STACK(data, d_key_idx)->next_sibling;
      Token key = JSON_NODE_FROM_STACK(data, d_key_idx)->token;
      int s_prop_idx = -1;
      int s_curr = s_node->props_head;
      while (s_curr != -1) {
        if (token_equals(key, SCH_NODE_FROM_STACK(schema, s_curr)->name)) {
          s_prop_idx = s_curr;
          break;
        }
        s_curr = SCH_NODE_FROM_STACK(schema, s_curr)->next_sibling;
      }

      if (s_prop_idx != -1) {
        int res =
            validate_recursive(data, d_val_idx, schema, s_prop_idx, error);
        if (res != -1) {
          error_res = res;
          goto end;
        }
      } else if (s_node->additional_schema != -1) {
        int res = validate_recursive(data, d_val_idx, schema,
                                     s_node->additional_schema, error);
        if (res != -1) {
          error_res = res;
          goto end;
        }
      }

      d_key_idx = JSON_NODE_FROM_STACK(data, d_val_idx)->next_sibling;
    }
  } else if (s_node->type == SC_ARRAY && d_node->type == AST_ARRAY) {
    if (s_node->items_head != -1) {
      int d_curr = d_node->first_child;
      while (d_curr != -1) {
        int res =
            validate_recursive(data, d_curr, schema, s_node->items_head, error);
        if (res != -1) {
          error_res = res;
          goto end;
        }
        d_curr = JSON_NODE_FROM_STACK(data, d_curr)->next_sibling;
      }
    }
  }

end:
  return error_res;
  /*#endregion*/
}

int validate_against_schema(Stack *data, int data_root, Stack *schema,
                            int schema_root, E **error) {
  /*#region*/
  return validate_recursive(data, data_root, schema, schema_root, error);
  /*#endregion*/
}
