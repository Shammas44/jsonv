#include "validate.h"
// #include "assert.h"
// #include "ast.h"
// #include "schema.h"
// #include <regex.h>
// #include <stdbool.h>
// #include <stdio.h>
// #include <stdlib.h>
// #include <string.h>

// #define VALIDATION_ERROR(t, fmt, ...)                                          \
//   do {                                                                         \
//     (*(error))->type = t;                                                      \
//     snprintf((*(error))->description, 100, fmt, __VA_ARGS__);                  \
//   } while (0)

// #define CMPPROP(c, lit)                                     \
//     (strncmp((const char *)(c)->name.start,                       \
//              (lit),                                               \
//              (c)->name.length) == 0)

// // Macros for safe access
// #define SCH_NODE(s, i) (&((SchemaNode *)(s)->data)[i])
// #define JSON_NODE(s, i) (&((ASTNode *)(s)->data)[i])

// // Helper: Parse double from Token (for numeric comparisons)
// static double parse_number_token(Token t) { return t.value.number; }

// static bool token_equals(Token a, Token b) {
//   /*#region*/
//   // Simple equality check
//   // If one is quoted and one isn't, you might need normalization logic here.
//   // For now, assuming both come from similar sources or are normalized.
//   if (a.value.string.length != b.value.string.length)
//     return false;
//   return strncmp((char *)a.value.string.start, (char *)b.value.string.start,
//                  a.value.string.length) == 0;
//   /*#endregion*/
// }

// static bool match_pattern(Token pattern, Token value) {
//   /*#region*/
//   regex_t regex;
//   char pat_str[256];
//   printf("=== pattern: %.*s\n", (int)pattern.value.string.length,pattern.value.string.start);
//   
//   // Safe copy of pattern (ensure null-termination for regcomp)
//   size_t pat_len = pattern.value.string.length < 255 ? pattern.value.string.length : 255;
//   memcpy(pat_str, pattern.value.string.start, pat_len);
//   pat_str[pat_len] = '\0';

//   if (regcomp(&regex, pat_str, REG_EXTENDED | REG_NOSUB) != 0) {
//     return false; // Invalid regex in schema treated as no-match (or could error)
//   }

//   // Use REG_STARTEND to match against non-null-terminated Token strings
//   regmatch_t m;
//   m.rm_so = 0;
//   m.rm_eo = value.value.string.length;

//   int rc = regexec(&regex, (const char *)value.value.string.start, 0, NULL, REG_STARTEND);
//   
//   regfree(&regex);
//   return (rc == 0);
//   /*#endregion*/
// }

// static int check_constraints(int d_node_idx, SchemaNode *s_node,
//                              Stack *schema_stack, Stack *data_stack,
//                              E **error) {
//   /*#region*/
//   (void)(schema_stack);
//   ASTNode *d_node = JSON_NODE_FROM_STACK(data_stack, d_node_idx);

//   for (int i = 0; i < s_node->constraints_count; i++) {
//     Constraint *c = &s_node->constraints[i];

//     if (c->type == T_NUMBER) {

//       if (CMPPROP(c,"minItems") && d_node->type == AST_ARRAY) {
//         int count = 0;
//         int curr = d_node->first_child;
//         while (curr != -1) {
//           count++;
//           curr = JSON_NODE_FROM_STACK(data_stack, curr)->next_sibling;
//         }
//         if (count < c->value.number) {
//           VALIDATION_ERROR(Jsonv_MinItems_error,
//                            "Array too short (minItems %.0f).", c->value.number);
//           return d_node_idx;
//         }
//       }

//       else if (CMPPROP(c, "minimum")) {
//         double input = d_node->token.value.number;
//         if (input < c->value.number) {
//           VALIDATION_ERROR(Jsonv_Minimum_error,
//                            "Value too small, expected >= %.2f, got %.2f",
//                            c->value.number, input);
//           return d_node_idx;
//         }
//       }

//       else if (CMPPROP(c, "maximum")) {
//         double input = parse_number_token(d_node->token);
//         if (input > c->value.number) {
//           VALIDATION_ERROR(Jsonv_Maximum_error,
//                            "Value too large, expected <= %.2f, got %.2f",
//                            c->value.number, input);
//           return d_node_idx;
//         }
//       }

//       else if (CMPPROP(c,"multipleOf")) {
//         int input = parse_number_token(d_node->token);
//         if (input % (int)c->value.number != 0) {
//           VALIDATION_ERROR(Jsonv_MultipleOf_error,
//                            "Expected %d to be multiple of %d.", input,
//                            (int)c->value.number);
//           return d_node_idx;
//         }
//       }

//       else if (CMPPROP(c,"exclusiveMaximum")) {
//         double input = parse_number_token(d_node->token);
//         if (input >= c->value.number) {
//           VALIDATION_ERROR(Jsonv_ExclusiveMaximum_error,
//                            "Value too large, expected < %.2f, got %.2f.",
//                            c->value.number, input);
//           return d_node_idx;
//         }
//       }

//       else if (CMPPROP(c,"exclusiveMinimum")) {
//         double input = parse_number_token(d_node->token);
//         if (input <= c->value.number) {
//           VALIDATION_ERROR(Jsonv_ExclusiveMinimum_error,
//                            "Value too small, expected > %.2f, got %.2f.",
//                            c->value.number, input);
//           return d_node_idx;
//         }
//       }

//     } else if (c->type == T_STRING) {

//       if (CMPPROP(c,"minLength")) {
//         int actual_len = d_node->token.value.string.length;
//         if (actual_len < c->value.number) {
//           VALIDATION_ERROR(Jsonv_MinLength_error,
//                            "String too short, expected >= %.0f, got %d.",
//                            c->value.number, actual_len);
//           return d_node_idx;
//         }
//       }

//       else if (CMPPROP(c,"maxLength")) {
//         int actual_len = d_node->token.value.string.length;
//         if (actual_len > c->value.number) {
//           VALIDATION_ERROR(Jsonv_MaxLength_error,
//                            "String too long, expected <= %.0f, got %d.",
//                            c->value.number, actual_len);
//           return d_node_idx;
//         }
//       }

//       else if (CMPPROP(c,"pattern")) {
//         size_t actual_len = d_node->token.value.string.length;
//         const char *value = (const char *)d_node->token.value.string.start;
//         regex_t regex;
//         int rc;
//         regmatch_t m;
//         /* REG_STARTEND requires this */
//         m.rm_so = 0;
//         m.rm_eo = actual_len;

//         char schema_pattern[100];

//         /* Safe, guaranteed NUL-terminated copy */
//         size_t pat_len = c->value.string.length;
//         if (pat_len >= sizeof(schema_pattern)) {
//           VALIDATION_ERROR(Jsonv_Compile_Regexp_Failed,
//                            "Schema regexp too long.", NULL);
//           return d_node_idx;
//         }

//         memcpy(schema_pattern, c->value.string.start, pat_len);
//         schema_pattern[pat_len] = '\0';
//         rc = regcomp(&regex, schema_pattern, REG_EXTENDED);
//         if (rc != 0) {
//           VALIDATION_ERROR(Jsonv_Compile_Regexp_Failed,
//                            "Cannot compile schema regexp.", NULL);
//           return d_node_idx;
//         }

//         rc = regexec(&regex, value, 1, &m, REG_STARTEND);

//         regfree(&regex);

//         if (rc != 0) {
//           VALIDATION_ERROR(Jsonv_Pattern_error, "Pattern matching failed.",
//                            NULL);
//           return d_node_idx;
//         }
//       }
//     }
//   }
//   return JSONV_SCHEMA_IS_VALID;
//   /*#endregion*/
// }

// static int validate_recursive(Stack *data, int data_idx, Stack *schema,
//                               int schema_idx, E **error) {
//   /*#region*/
//   if (data_idx == -1 || schema_idx == -1)
//     return JSONV_SCHEMA_IS_VALID;

//   ASTNode *d_node = JSON_NODE_FROM_STACK(data, data_idx);
//   SchemaNode *s_node = SCH_NODE_FROM_STACK(schema, schema_idx);

//   // 0. Immediate Fail
//   if (s_node->type == SC_FALSE) {
//     VALIDATION_ERROR(Jsonv_ValueNotAllowed_error,
//                      "Value not allowed (Schema is false).", NULL);
//     return data_idx;
//   }
//   if (s_node->type == SC_ANY)
//     return JSONV_SCHEMA_IS_VALID;

//   // 1. Type Match
//   bool type_match = false;
//   switch (s_node->type) {
//   case SC_STRING: type_match = (d_node->token.type == T_STRING); break;
//   case SC_NUMBER: type_match = (d_node->token.type == T_NUMBER); break;
//   case SC_BOOLEAN: type_match = (d_node->token.type == T_TRUE || d_node->token.type == T_FALSE); break;
//   case SC_NULL: type_match = (d_node->token.type == T_NULL); break;
//   case SC_OBJECT: type_match = (d_node->type == AST_OBJECT); break;
//   case SC_ARRAY: type_match = (d_node->type == AST_ARRAY); break;
//   default: type_match = true; break;
//   }

//   if (!type_match) {
//     VALIDATION_ERROR(Jsonv_Type_error, "Type mismatch, expected type %d.", s_node->type);
//     return data_idx;
//   }

//   // 2. Constraints
//   if (check_constraints(data_idx, s_node, schema, data, error) != -1) {
//     return data_idx;
//   }

//   // 3. Structure
//   if (s_node->type == SC_OBJECT && d_node->type == AST_OBJECT) {
//     
//     // A. Required Fields
//     int req = s_node->required_head;
//     while (req != -1) {
//       Token r_name = SCH_NODE_FROM_STACK(schema, req)->name;
//       bool found = false;
//       int child = d_node->first_child;
//       while (child != -1) {
//         if (token_equals(JSON_NODE_FROM_STACK(data, child)->token, r_name)) {
//           found = true;
//           break;
//         }
//         int val = JSON_NODE_FROM_STACK(data, child)->next_sibling;
//         child = JSON_NODE_FROM_STACK(data, val)->next_sibling;
//       }
//       if (!found) {
//         VALIDATION_ERROR(Jsonv_Required_error, "Missing required field '%.*s'.",
//                          (int)r_name.value.string.length, r_name.value.string.start);
//         return data_idx;
//       }
//       req = SCH_NODE_FROM_STACK(schema, req)->next_sibling;
//     }

//     // B. Children (Properties, PatternProperties, AdditionalProperties)
//     int d_key_idx = d_node->first_child;
//     while (d_key_idx != -1) {
//       int d_val_idx = JSON_NODE_FROM_STACK(data, d_key_idx)->next_sibling;
//       Token key = JSON_NODE_FROM_STACK(data, d_key_idx)->token;
//       
//       bool matched_any = false;

//       // i. Check 'properties'
//       int s_curr = s_node->props_head;
//       while (s_curr != -1) {
//         if (token_equals(key, SCH_NODE_FROM_STACK(schema, s_curr)->name)) {
//           matched_any = true;
//           int res = validate_recursive(data, d_val_idx, schema, s_curr, error);
//           if (res != -1) return res;
//           break; 
//         }
//         s_curr = SCH_NODE_FROM_STACK(schema, s_curr)->next_sibling;
//       }

//       // ii. Check 'patternProperties'
//       int p_curr = s_node->pattern_props_head;
//       while (p_curr != -1) {
//         SchemaNode *p_node = SCH_NODE_FROM_STACK(schema, p_curr);
//         if (match_pattern(p_node->name, key)) {
//           matched_any = true;
//           int res = validate_recursive(data, d_val_idx, schema, p_curr, error);
//           if (res != -1) return res;
//         }
//         p_curr = p_node->next_sibling;
//       }

//       // iii. Check 'additionalProperties'
//       // Only validated if NOT matched by properties AND NOT matched by patternProperties
//       if (!matched_any && s_node->additional_schema_head != -1) {
//          SchemaNode *add_schema = SCH_NODE_FROM_STACK(schema, s_node->additional_schema_head);
//          
//          // Special handling for "additionalProperties: false" to give a better error message
//          if (add_schema->type == SC_FALSE) {
//              VALIDATION_ERROR(Jsonv_AdditionalProperties_error,
//                               "Additional property '%.*s' not allowed.",
//                               (int)key.value.string.length, key.value.string.start);
//              return d_key_idx;
//          }

//         int res = validate_recursive(data, d_val_idx, schema,
//                                      s_node->additional_schema_head, error);
//         if (res != JSONV_SCHEMA_IS_VALID) return res;
//       }

//       // Advance to next property in data
//       d_key_idx = JSON_NODE_FROM_STACK(data, d_val_idx)->next_sibling;
//     }
//   } 
//   else if (s_node->type == SC_ARRAY && d_node->type == AST_ARRAY) {
//     if (s_node->items_head != -1) {
//       int d_curr = d_node->first_child;
//       while (d_curr != -1) {
//         int res = validate_recursive(data, d_curr, schema, s_node->items_head, error);
//         if (res != -1) return res;
//         d_curr = JSON_NODE_FROM_STACK(data, d_curr)->next_sibling;
//       }
//     }
//   }

//   return JSONV_SCHEMA_IS_VALID;
//   /*#endregion*/
// }

// int validate_against_schema(Stack *data, int data_root, Stack *schema,
//                             int schema_root, E **error) {
//   /*#region*/
//   return validate_recursive(data, data_root, schema, schema_root, error);
//   /*#endregion*/
// }
