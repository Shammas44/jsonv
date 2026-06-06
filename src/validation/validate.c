#include "validate.h"
#include "shape.internal.h"
#include "schema.h"
#include "ctx.h"
#include "parser.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

      default:
        // Invalid opcode
        return false;
    }
  }

  return true;
  /*#endregion*/
}
