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

static bool regex_matches_key(const char *k_start, size_t k_len, Token pat_token, Jsonv_Context *ctx, const char *path, E *out_err) {
  /*#region*/
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

  char *target_str = (char *)jsonv_arena_alloc(jsonv_ctx_arena(ctx), k_len + 1);
  if (!target_str) return false;
  memcpy(target_str, k_start, k_len);
  target_str[k_len] = '\0';

  regex_t regex;
  if (regcomp(&regex, pattern_str, REG_EXTENDED | REG_NOSUB) != 0) {
    out_err->type = Jsonv_Compile_Regexp_Failed;
    out_err->path = path;
    snprintf(out_err->description, sizeof(out_err->description), "Failed to compile regex pattern '%s'.", pattern_str);
    return false;
  }

  int match_res = regexec(&regex, target_str, 0, NULL, 0);
  regfree(&regex);

  return match_res == 0;
  /*#endregion*/
}

static bool validate_ipv4(const char *s, size_t len) {
  /*#region*/
  int parts = 0;
  int current_val = 0;
  bool part_started = false;
  for (size_t i = 0; i < len; i++) {
    char c = s[i];
    if (c >= '0' && c <= '9') {
      if (current_val == 0 && part_started) {
        return false;
      }
      current_val = current_val * 10 + (c - '0');
      if (current_val > 255) return false;
      part_started = true;
    } else if (c == '.') {
      if (!part_started) return false;
      parts++;
      current_val = 0;
      part_started = false;
    } else {
      return false;
    }
  }
  return parts == 3 && part_started;
  /*#endregion*/
}

static bool validate_email(const char *s, size_t len) {
  /*#region*/
  int at_idx = -1;
  for (size_t i = 0; i < len; i++) {
    if (s[i] == '@') {
      if (at_idx != -1) return false;
      at_idx = (int)i;
    }
  }
  if (at_idx <= 0 || at_idx >= (int)len - 1) return false;
  bool dot_found = false;
  for (size_t i = at_idx + 2; i < len - 1; i++) {
    if (s[i] == '.') {
      dot_found = true;
      break;
    }
  }
  return dot_found;
  /*#endregion*/
}

static bool validate_uuid(const char *s, size_t len) {
  /*#region*/
  if (len != 36) return false;
  for (size_t i = 0; i < 36; i++) {
    char c = s[i];
    if (i == 8 || i == 13 || i == 18 || i == 23) {
      if (c != '-') return false;
    } else {
      if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'))) {
        return false;
      }
    }
  }
  return true;
  /*#endregion*/
}

static bool validate_datetime(const char *s, size_t len) {
  /*#region*/
  if (len < 20) return false;
  if (s[4] != '-' || s[7] != '-' || (s[10] != 'T' && s[10] != 't') || s[13] != ':' || s[16] != ':') return false;
  for (int i = 0; i < 19; i++) {
    if (i != 4 && i != 7 && i != 10 && i != 13 && i != 16) {
      if (s[i] < '0' || s[i] > '9') return false;
    }
  }
  int year = (s[0]-'0')*1000 + (s[1]-'0')*100 + (s[2]-'0')*10 + (s[3]-'0');
  int month = (s[5]-'0')*10 + (s[6]-'0');
  int day = (s[8]-'0')*10 + (s[9]-'0');
  int hour = (s[11]-'0')*10 + (s[12]-'0');
  int minute = (s[14]-'0')*10 + (s[15]-'0');
  int second = (s[17]-'0')*10 + (s[18]-'0');

  if (month < 1 || month > 12) return false;
  if (day < 1 || day > 31) return false;
  if (hour > 23) return false;
  if (minute > 59) return false;
  if (second > 60) return false;

  if (month == 4 || month == 6 || month == 9 || month == 11) {
    if (day > 30) return false;
  }
  if (month == 2) {
    bool is_leap = (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0));
    if (is_leap) {
      if (day > 29) return false;
    } else {
      if (day > 28) return false;
    }
  }

  if (len > 19) {
    if (s[19] == '.') {
      size_t idx = 20;
      while (idx < len && s[idx] >= '0' && s[idx] <= '9') {
        idx++;
      }
      if (idx == len) return false;
      if (s[idx] == 'Z' || s[idx] == 'z') {
        return idx == len - 1;
      }
      if (s[idx] == '+' || s[idx] == '-') {
        if (len - idx != 6) return false;
        if (s[idx+3] != ':') return false;
        return (s[idx+1] >= '0' && s[idx+1] <= '9') && (s[idx+2] >= '0' && s[idx+2] <= '9') &&
               (s[idx+4] >= '0' && s[idx+4] <= '9') && (s[idx+5] >= '0' && s[idx+5] <= '9');
      }
      return false;
    } else if (s[19] == 'Z' || s[19] == 'z') {
      return len == 20;
    } else if (s[19] == '+' || s[19] == '-') {
      if (len != 25) return false;
      if (s[22] != ':') return false;
      return (s[20] >= '0' && s[20] <= '9') && (s[21] >= '0' && s[21] <= '9') &&
             (s[23] >= '0' && s[23] <= '9') && (s[24] >= '0' && s[24] <= '9');
    }
    return false;
  }
  return true;
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
        /*#region*/
        uint32_t prop_count = read_uint32(&pc);
        uint32_t pattern_prop_count = read_uint32(&pc);
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

        DecodedPropertyRule *pattern_props = NULL;
        DecodedPropertyRule stack_pattern_props[64];
        if (pattern_prop_count <= 64) {
          pattern_props = stack_pattern_props;
        } else {
          pattern_props = (DecodedPropertyRule *)jsonv_arena_alloc(jsonv_ctx_arena(ctx), pattern_prop_count * sizeof(DecodedPropertyRule));
          if (!pattern_props) return false;
        }

        for (uint32_t k = 0; k < pattern_prop_count; k++) {
          pattern_props[k].key = read_token(&pc);
          pattern_props[k].rule_offset = read_uint32(&pc);
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

              bool matched = false;

              // Match against properties
              for (uint32_t p = 0; p < prop_count; p++) {
                if (key_matches_token_ast(k_start, k_len, props[p].key)) {
                  matched = true;
                  if (val_idx != -1) {
                    if (!validate_bytecode(ctx, pool, schema, props[p].rule_offset, val_idx, new_path ? new_path : path, out_err)) {
                      return false;
                    }
                  }
                  break;
                }
              }

              // Match against patternProperties
              bool compile_failed = false;
              for (uint32_t p = 0; p < pattern_prop_count; p++) {
                E temp_err = {0};
                if (regex_matches_key(k_start, k_len, pattern_props[p].key, ctx, path, &temp_err)) {
                  matched = true;
                  if (val_idx != -1) {
                    if (!validate_bytecode(ctx, pool, schema, pattern_props[p].rule_offset, val_idx, new_path ? new_path : path, out_err)) {
                      return false;
                    }
                  }
                } else if (temp_err.type == Jsonv_Compile_Regexp_Failed) {
                  *out_err = temp_err;
                  compile_failed = true;
                  break;
                }
              }
              if (compile_failed) return false;

              // Match against additionalProperties if not matched by either
              if (!matched) {
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
        /*#endregion*/
      }

      case OP_PROPERTY_NAMES: {
        /*#region*/
        uint32_t sub_offset = read_uint32(&pc);
        if (node->type == AST_OBJECT) {
          int curr = node->first_child;
          while (curr != -1) {
            if (pool[curr].type != AST_SKIPPED) {
              Token k = pool[curr].token;
              const char *k_start = (const char *)k.value.string.start;
              size_t k_len = k.value.string.length;

              // Construct new path
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

              if (!validate_bytecode(ctx, pool, schema, sub_offset, curr, new_path ? new_path : path, out_err)) {
                return false;
              }
            }
            int val_idx = pool[curr].next_sibling;
            if (val_idx == -1) break;
            curr = pool[val_idx].next_sibling;
          }
        }
        break;
        /*#endregion*/
      }

      case OP_FORMAT: {
        /*#region*/
        Token format_token = read_token(&pc);
        if (node->type == AST_LEAF && node->token.type == T_STRING) {
          const char *val_start = (const char *)node->token.value.string.start;
          size_t val_len = node->token.value.string.length;
          if (val_len >= 2 && val_start[0] == '"' && val_start[val_len - 1] == '"') {
            val_start++;
            val_len -= 2;
          }

          const char *fmt_start = (const char *)format_token.value.string.start;
          size_t fmt_len = format_token.value.string.length;
          if (fmt_len >= 2 && fmt_start[0] == '"' && fmt_start[fmt_len - 1] == '"') {
            fmt_start++;
            fmt_len -= 2;
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
            out_err->type = Jsonv_Format_error;
            out_err->path = path;
            snprintf(out_err->description, sizeof(out_err->description), "String '%.*s' does not conform to format '%.*s'.", (int)val_len, val_start, (int)fmt_len, fmt_start);
            return false;
          }
        }
        break;
        /*#endregion*/
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

      case OP_ALL_OF: {
        /*#region*/
        uint32_t count = read_uint32(&pc);
        bool all_valid = true;
        for (uint32_t i = 0; i < count; i++) {
          uint32_t sub_offset = read_uint32(&pc);
          if (all_valid) {
            E temp_err = {0};
            if (!validate_bytecode(ctx, pool, schema, sub_offset, node_idx, path, &temp_err)) {
              all_valid = false;
            }
          }
        }
        if (!all_valid) {
          out_err->type = Jsonv_AllOf_error;
          out_err->path = path;
          snprintf(out_err->description, sizeof(out_err->description), "Value must validate against all subschemas in allOf.");
          return false;
        }
        break;
        /*#endregion*/
      }

      case OP_ANY_OF: {
        /*#region*/
        uint32_t count = read_uint32(&pc);
        bool any_valid = false;
        for (uint32_t i = 0; i < count; i++) {
          uint32_t sub_offset = read_uint32(&pc);
          if (!any_valid) {
            E temp_err = {0};
            if (validate_bytecode(ctx, pool, schema, sub_offset, node_idx, path, &temp_err)) {
              any_valid = true;
            }
          }
        }
        if (!any_valid) {
          out_err->type = Jsonv_AnyOf_error;
          out_err->path = path;
          snprintf(out_err->description, sizeof(out_err->description), "Value must validate against at least one subschema in anyOf.");
          return false;
        }
        break;
        /*#endregion*/
      }

      case OP_ONE_OF: {
        /*#region*/
        uint32_t count = read_uint32(&pc);
        uint32_t valid_count = 0;
        for (uint32_t i = 0; i < count; i++) {
          uint32_t sub_offset = read_uint32(&pc);
          E temp_err = {0};
          if (validate_bytecode(ctx, pool, schema, sub_offset, node_idx, path, &temp_err)) {
            valid_count++;
          }
        }
        if (valid_count != 1) {
          out_err->type = Jsonv_OneOf_error;
          out_err->path = path;
          snprintf(out_err->description, sizeof(out_err->description), "Value must validate against exactly one subschema in oneOf (validated against %u).", valid_count);
          return false;
        }
        break;
        /*#endregion*/
      }

      case OP_IF_THEN_ELSE: {
        /*#region*/
        uint32_t if_offset = read_uint32(&pc);
        uint32_t then_offset = read_uint32(&pc);
        uint32_t else_offset = read_uint32(&pc);

        E temp_err = {0};
        bool if_passed = validate_bytecode(ctx, pool, schema, if_offset, node_idx, path, &temp_err);
        if (if_passed) {
          if (then_offset != (uint32_t)-1) {
            if (!validate_bytecode(ctx, pool, schema, then_offset, node_idx, path, out_err)) {
              return false;
            }
          }
        } else {
          if (else_offset != (uint32_t)-1) {
            if (!validate_bytecode(ctx, pool, schema, else_offset, node_idx, path, out_err)) {
              return false;
            }
          }
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
