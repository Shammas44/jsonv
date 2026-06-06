#include "validate.h"
#include "shape.internal.h"
#include "schema.h"
#include "ctx.h"
#include "parser.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <regex.h>

struct Jsonv_Schema {
  uint8_t *bytecode;
  uint32_t length;
};

typedef struct {
  Token key;
  uint32_t rule_offset;
} DecodedPropertyRule;

/*#region Bytecode Reading Helpers*/
static inline uint8_t read_byte(const uint8_t **pc) {
  /*#region*/
  uint8_t val = **pc;
  (*pc)++;
  return val;
  /*#endregion*/
}

static inline uint32_t read_uint32(const uint8_t **pc) {
  /*#region*/
  uint32_t val;
  memcpy(&val, *pc, sizeof(val));
  *pc += sizeof(val);
  return val;
  /*#endregion*/
}

static inline int32_t read_int32(const uint8_t **pc) {
  /*#region*/
  int32_t val;
  memcpy(&val, *pc, sizeof(val));
  *pc += sizeof(val);
  return val;
  /*#endregion*/
}

static inline double read_double(const uint8_t **pc) {
  /*#region*/
  double val;
  memcpy(&val, *pc, sizeof(val));
  *pc += sizeof(val);
  return val;
  /*#endregion*/
}

static inline Token read_token(const uint8_t **pc) {
  /*#region*/
  Token val;
  memcpy(&val, *pc, sizeof(val));
  *pc += sizeof(val);
  return val;
  /*#endregion*/
}
/*#endregion*/

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

static bool ast_nodes_equal(const ASTNode *pool, int n1_idx, int n2_idx) {
  /*#region*/
  if (n1_idx == -1 && n2_idx == -1) return true;
  if (n1_idx == -1 || n2_idx == -1) return false;

  const ASTNode *n1 = &pool[n1_idx];
  const ASTNode *n2 = &pool[n2_idx];

  ASTNodeType type1 = n1->type;
  ASTNodeType type2 = n2->type;

  if (type1 == AST_LEAF) {
    if (n1->token.type == T_STRING) type1 = AST_STRING;
    else if (n1->token.type == T_NUMBER) type1 = AST_NUMBER;
    else if (n1->token.type == T_NULL) type1 = AST_NULL;
    else if (n1->token.type == T_TRUE) type1 = AST_TRUE;
    else if (n1->token.type == T_FALSE) type1 = AST_FALSE;
  }
  if (type2 == AST_LEAF) {
    if (n2->token.type == T_STRING) type2 = AST_STRING;
    else if (n2->token.type == T_NUMBER) type2 = AST_NUMBER;
    else if (n2->token.type == T_NULL) type2 = AST_NULL;
    else if (n2->token.type == T_TRUE) type2 = AST_TRUE;
    else if (n2->token.type == T_FALSE) type2 = AST_FALSE;
  }

  if (type1 != type2) return false;

  switch (type1) {
    case AST_NULL:
    case AST_TRUE:
    case AST_FALSE:
      return true;

    case AST_NUMBER:
      return n1->token.value.number == n2->token.value.number;

    case AST_STRING: {
      const char *s1 = (const char *)n1->token.value.string.start;
      size_t len1 = n1->token.value.string.length;
      const char *s2 = (const char *)n2->token.value.string.start;
      size_t len2 = n2->token.value.string.length;
      
      if (len1 >= 2 && s1[0] == '"' && s1[len1 - 1] == '"') {
        s1++;
        len1 -= 2;
      }
      if (len2 >= 2 && s2[0] == '"' && s2[len2 - 1] == '"') {
        s2++;
        len2 -= 2;
      }
      if (len1 != len2) return false;
      return memcmp(s1, s2, len1) == 0;
    }

    case AST_ARRAY: {
      int c1 = n1->first_child;
      int c2 = n2->first_child;
      while (c1 != -1 && c2 != -1) {
        while (c1 != -1 && pool[c1].type == AST_SKIPPED) {
          c1 = pool[c1].next_sibling;
        }
        while (c2 != -1 && pool[c2].type == AST_SKIPPED) {
          c2 = pool[c2].next_sibling;
        }
        if (c1 == -1 && c2 == -1) break;
        if (c1 == -1 || c2 == -1) return false;
        if (!ast_nodes_equal(pool, c1, c2)) return false;
        c1 = pool[c1].next_sibling;
        c2 = pool[c2].next_sibling;
      }
      while (c1 != -1 && pool[c1].type == AST_SKIPPED) {
        c1 = pool[c1].next_sibling;
      }
      while (c2 != -1 && pool[c2].type == AST_SKIPPED) {
        c2 = pool[c2].next_sibling;
      }
      return (c1 == -1 && c2 == -1);
    }

    case AST_OBJECT: {
      int count1 = 0;
      int c1 = n1->first_child;
      while (c1 != -1) {
        int val1_idx = pool[c1].next_sibling;
        if (pool[c1].type != AST_SKIPPED) {
          count1++;
          const ASTNode *key1 = &pool[c1];
          const char *k1_start = (const char *)key1->token.value.string.start;
          size_t k1_len = key1->token.value.string.length;
          if (k1_len >= 2 && k1_start[0] == '"' && k1_start[k1_len - 1] == '"') {
            k1_start++;
            k1_len -= 2;
          }
          
          int c2 = n2->first_child;
          int found_idx = -1;
          while (c2 != -1) {
            int val2_idx = pool[c2].next_sibling;
            if (pool[c2].type != AST_SKIPPED) {
              const ASTNode *key2 = &pool[c2];
              const char *k2_start = (const char *)key2->token.value.string.start;
              size_t k2_len = key2->token.value.string.length;
              if (k2_len >= 2 && k2_start[0] == '"' && k2_start[k2_len - 1] == '"') {
                k2_start++;
                k2_len -= 2;
              }
              if (k1_len == k2_len && memcmp(k1_start, k2_start, k1_len) == 0) {
                found_idx = val2_idx;
                break;
              }
            }
            c2 = pool[val2_idx].next_sibling;
          }
          if (found_idx == -1) return false;
          if (!ast_nodes_equal(pool, val1_idx, found_idx)) return false;
        }
        c1 = pool[val1_idx].next_sibling;
      }
      
      int count2 = 0;
      int c2 = n2->first_child;
      while (c2 != -1) {
        int val2_idx = pool[c2].next_sibling;
        if (pool[c2].type != AST_SKIPPED) {
          count2++;
        }
        c2 = pool[val2_idx].next_sibling;
      }
      return count1 == count2;
    }

    default:
      return false;
  }
  /*#endregion*/
}

/* Opcode execution loop on ASTNode pool index */
bool validate_bytecode(
    Jsonv_Context *ctx,
    ASTNode *pool,
    const Jsonv_Schema *schema,
    uint32_t offset,
    int node_idx,
    const char *path,
    E *out_err
) {
  /*#region*/
  if (offset >= schema->length) return true;
  
  const uint8_t *pc = schema->bytecode + offset;
  ASTNode *node = &pool[node_idx];

  bool running = true;
  while (running) {
    uint8_t opcode = read_byte(&pc);
    switch (opcode) {
      case OP_END: {
        running = false;
        break;
      }

      case OP_FAIL: {
        out_err->type = Jsonv_ValueNotAllowed_error;
        out_err->path = path;
        snprintf(out_err->description, sizeof(out_err->description), "Value not allowed (Schema is false).");
        return false;
      }

      case OP_TYPE: {
        uint32_t type_mask = read_uint32(&pc);
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
        break;
      }

      case OP_MINIMUM: {
        double min_val = read_double(&pc);
        if (node->type == AST_LEAF && node->token.type == T_NUMBER) {
          double num = node->token.value.number;
          if (num < min_val) {
            out_err->type = Jsonv_Minimum_error;
            out_err->path = path;
            snprintf(out_err->description, sizeof(out_err->description), "Value too small, expected >= %.2f, got %.2f.", min_val, num);
            return false;
          }
        }
        break;
      }

      case OP_MAXIMUM: {
        double max_val = read_double(&pc);
        if (node->type == AST_LEAF && node->token.type == T_NUMBER) {
          double num = node->token.value.number;
          if (num > max_val) {
            out_err->type = Jsonv_Maximum_error;
            out_err->path = path;
            snprintf(out_err->description, sizeof(out_err->description), "Value too large, expected <= %.2f, got %.2f.", max_val, num);
            return false;
          }
        }
        break;
      }

      case OP_MULTIPLE_OF: {
        double mult_val = read_double(&pc);
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
        break;
      }

      case OP_EXCLUSIVE_MINIMUM: {
        double min_val = read_double(&pc);
        if (node->type == AST_LEAF && node->token.type == T_NUMBER) {
          double num = node->token.value.number;
          if (num <= min_val) {
            out_err->type = Jsonv_ExclusiveMinimum_error;
            out_err->path = path;
            snprintf(out_err->description, sizeof(out_err->description), "Value too small, expected > %.2f, got %.2f.", min_val, num);
            return false;
          }
        }
        break;
      }

      case OP_EXCLUSIVE_MAXIMUM: {
        double max_val = read_double(&pc);
        if (node->type == AST_LEAF && node->token.type == T_NUMBER) {
          double num = node->token.value.number;
          if (num >= max_val) {
            out_err->type = Jsonv_ExclusiveMaximum_error;
            out_err->path = path;
            snprintf(out_err->description, sizeof(out_err->description), "Value too large, expected < %.2f, got %.2f.", max_val, num);
            return false;
          }
        }
        break;
      }

      case OP_PATTERN: {
        Token pat_token = read_token(&pc);
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

          const char *pat_start = (const char *)pat_token.value.string.start;
          size_t pat_len = pat_token.value.string.length;
          if (pat_len >= 2 && pat_start[0] == '"' && pat_start[pat_len - 1] == '"') {
            pat_start++;
            pat_len -= 2;
          }
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
        break;
      }

      case OP_MIN_LENGTH: {
        int32_t min_len = read_int32(&pc);
        if (node->type == AST_LEAF && node->token.type == T_STRING) {
          size_t len = node->token.value.string.length;
          if (len < (size_t)min_len) {
            out_err->type = Jsonv_MinLength_error;
            out_err->path = path;
            snprintf(out_err->description, sizeof(out_err->description), "String too short, expected >= %d, got %lu.", min_len, len);
            return false;
          }
        }
        break;
      }

      case OP_MAX_LENGTH: {
        int32_t max_len = read_int32(&pc);
        if (node->type == AST_LEAF && node->token.type == T_STRING) {
          size_t len = node->token.value.string.length;
          if (len > (size_t)max_len) {
            out_err->type = Jsonv_MaxLength_error;
            out_err->path = path;
            snprintf(out_err->description, sizeof(out_err->description), "String too long, expected <= %d, got %lu.", max_len, len);
            return false;
          }
        }
        break;
      }

      case OP_MIN_PROPERTIES: {
        int32_t min_props = read_int32(&pc);
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
        break;
      }

      case OP_MAX_PROPERTIES: {
        int32_t max_props = read_int32(&pc);
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
        break;
      }

      case OP_MIN_ITEMS: {
        int32_t min_items = read_int32(&pc);
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
        break;
      }

      case OP_MAX_ITEMS: {
        int32_t max_items = read_int32(&pc);
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
        break;
      }

      case OP_ITEMS: {
        uint32_t items_offset = read_uint32(&pc);
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
        break;
      }

      case OP_REQUIRED: {
        uint32_t count = read_uint32(&pc);
        if (node->type == AST_OBJECT) {
          for (uint32_t i = 0; i < count; i++) {
            Token t = read_token(&pc);
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
        } else {
          pc += count * sizeof(Token);
        }
        break;
      }

      case OP_PROPERTIES: {
        uint32_t prop_count = read_uint32(&pc);
        int32_t additional_props_rule = read_int32(&pc);

        DecodedPropertyRule *props = NULL;
        DecodedPropertyRule stack_props[64];
        if (prop_count <= 64) {
          props = stack_props;
        } else {
          props = (DecodedPropertyRule *)jsonv_arena_alloc(jsonv_ctx_arena(ctx), prop_count * sizeof(DecodedPropertyRule));
          if (!props) return false;
        }

        for (uint32_t k = 0; k < prop_count; k++) {
          props[k].key = read_token(&pc);
          props[k].rule_offset = read_uint32(&pc);
        }

        if (node->type == AST_OBJECT) {
          int curr = node->first_child;
          while (curr != -1) {
            if (pool[curr].type != AST_SKIPPED) {
              Token k = pool[curr].token;
              int val_idx = pool[curr].next_sibling;

              // Extract key string view
              const char *k_start = (const char *)k.value.string.start;
              size_t k_len = k.value.string.length;

              // Match against properties in the decoded list
              bool matched = false;
              uint32_t prop_rule_offset = 0;
              for (uint32_t p = 0; p < prop_count; p++) {
                if (key_matches_token_ast(k_start, k_len, props[p].key)) {
                  matched = true;
                  prop_rule_offset = props[p].rule_offset;
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
                  if (!validate_bytecode(ctx, pool, schema, prop_rule_offset, val_idx, new_path ? new_path : path, out_err)) {
                    return false;
                  }
                }
              } else {
                // Check additional properties rule
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
        break;
      }

      case OP_UNIQUE_ITEMS: {
        /*#region*/
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
        break;
        /*#endregion*/
      }

      case OP_CONTAINS: {
        /*#region*/
        uint32_t contains_offset = read_uint32(&pc);
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
        break;
        /*#endregion*/
      }

      case OP_NOT: {
        /*#region*/
        uint32_t not_offset = read_uint32(&pc);
        E temp_err = {0};
        if (validate_bytecode(ctx, pool, schema, not_offset, node_idx, path, &temp_err)) {
          out_err->type = Jsonv_Not_error;
          out_err->path = path;
          snprintf(out_err->description, sizeof(out_err->description), "Value must not validate against subschema.");
          return false;
        }
        break;
        /*#endregion*/
      }

      default:
        // Invalid opcode
        return false;
    }
  }

  return true;
  /*#endregion*/
}
