#include "validate_internal.h"

bool regex_matches_key(const char *k_start, size_t k_len, const char *pat_start, size_t pat_len, Jsonv_Context *ctx, const char *path, Jsonv_Error *out_err) {
  /*#region*/
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

bool validate_ipv4(const char *s, size_t len) {
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

bool validate_email(const char *s, size_t len) {
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

bool validate_uuid(const char *s, size_t len) {
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

bool validate_datetime(const char *s, size_t len) {
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

bool ast_nodes_equal(const ASTNode *pool, int n1_idx, int n2_idx) {
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
