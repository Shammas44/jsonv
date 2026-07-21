#include "yaml_lexer.h"
#ifdef JSONV_YAML_SUPPORT
#include "assert.h"
#include "mem.h"
#include <errno.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#define T YamlLexer

static void skip_spaces_on_line(T *l) {
  /*#region*/
  while (l->current_pos < l->source_len) {
    char c = l->source[l->current_pos];
    if (c == ' ' || c == '\t') {
      l->current_pos++;
      l->current_col++;
    } else {
      break;
    }
  }
  /*#endregion*/
}

static bool is_newline(char c) {
  /*#region*/
  return c == '\n' || c == '\r';
  /*#endregion*/
}

static Token parse_quoted_string(T *l, char quote) {
  /*#region*/
  l->current_pos++; // Consume opening quote
  l->current_col++;
  size_t start = l->current_pos;

  while (l->current_pos < l->source_len) {
    char c = l->source[l->current_pos];
    if (c == quote) {
      if (quote == '\'' && l->current_pos + 1 < l->source_len && l->source[l->current_pos + 1] == '\'') {
        // Escaped single quote via doubling ''
        l->current_pos += 2;
        l->current_col += 2;
        continue;
      }
      size_t len = l->current_pos - start;
      l->current_pos++; // Consume closing quote
      l->current_col++;
      return (Token){.value = {.string = {l->source + start, len}}, T_STRING};
    }
    if (c == '\\' && quote == '"') {
      // Escape char (minimal support for zero-copy views)
      l->current_pos++;
      l->current_col++;
    }
    l->current_pos++;
    l->current_col++;
  }
  return (Token){.value = {.string = {l->source + start, l->source_len - start}}, T_ERROR};
  /*#endregion*/
}

static Token parse_unquoted_scalar(T *l) {
  /*#region*/
  size_t start = l->current_pos;
  size_t end = start;

  while (l->current_pos < l->source_len) {
    char c = l->source[l->current_pos];
    
    // Stop at newline or comment
    if (is_newline(c) || c == '#') {
      break;
    }

    // Stop at flow markers only if we are in flow style (nested in { } or [ ])
    if (l->flow_depth > 0 && (c == ',' || c == '}' || c == ']')) {
      break;
    }

    // Stop at colon if it's followed by space, newline, or EOF
    if (c == ':') {
      if (l->current_pos + 1 >= l->source_len ||
          l->source[l->current_pos + 1] == ' ' ||
          l->source[l->current_pos + 1] == '\t' ||
          is_newline(l->source[l->current_pos + 1])) {
        break;
      }
    }

    l->current_pos++;
    l->current_col++;
    end = l->current_pos;
  }

  // Strip trailing spaces/tabs
  while (end > start && (l->source[end - 1] == ' ' || l->source[end - 1] == '\t')) {
    end--;
  }

  size_t len = end - start;
  const unsigned char *str_start = l->source + start;

  // Check for literals: true, false, null
  if (len == 4 && strncmp((const char *)str_start, "true", 4) == 0) {
    return (Token){{0}, T_TRUE};
  }
  if (len == 5 && strncmp((const char *)str_start, "false", 5) == 0) {
    return (Token){{0}, T_FALSE};
  }
  if (len == 4 && strncmp((const char *)str_start, "null", 4) == 0) {
    return (Token){{0}, T_NULL};
  }

  // Check if it's a number
  char buf[64];
  if (len < sizeof(buf)) {
    memcpy(buf, str_start, len);
    buf[len] = '\0';
    char *endptr;
    errno = 0;
    strtod(buf, &endptr);
    if (endptr != buf && *endptr == '\0' && errno == 0) {
      return (Token){.value = {.raw_number = {str_start, len}}, .type = T_NUMBER, .has_escape = false};
    }
  }

  return (Token){.value = {.string = {str_start, len}}, T_STRING};
  /*#endregion*/
}

static Token parse_block_scalar(T *l, char type) {
  /*#region*/
  (void)type; // both '|' and '>' yield raw block slice views in zero-copy mode
  
  l->current_pos++; // Consume opening symbol ('|' or '>')
  l->current_col++;
  
  // Consume any chomping (+/-) or indentation indicators to advance to inline space/comment scan
  while (l->current_pos < l->source_len) {
    char c = l->source[l->current_pos];
    if (c == '+' || c == '-' || (c >= '0' && c <= '9')) {
      l->current_pos++;
      l->current_col++;
    } else {
      break;
    }
  }
  
  // Strip trailing inline spaces/tabs on the header line to reach the line ending
  while (l->current_pos < l->source_len) {
    char c = l->source[l->current_pos];
    if (c == ' ' || c == '\t') {
      l->current_pos++;
      l->current_col++;
    } else {
      break;
    }
  }
  
  // Skip any trailing inline comment on the header line
  if (l->current_pos < l->source_len && l->source[l->current_pos] == '#') {
    while (l->current_pos < l->source_len && !is_newline(l->source[l->current_pos])) {
      l->current_pos++;
    }
  }
  
  // Consume the newline terminating the header line and reset column state
  if (l->current_pos < l->source_len && is_newline(l->source[l->current_pos])) {
    char nl = l->source[l->current_pos];
    l->current_pos++;
    if (nl == '\r' && l->current_pos < l->source_len && l->source[l->current_pos] == '\n') {
      l->current_pos++;
    }
    l->current_line++;
    l->current_col = 0;
  }
  
  int parent_indent = l->indent_stack[l->indent_top];
  int block_indent = -1;
  size_t block_start = 0;
  size_t block_end = 0;
  
  // Scan subsequent lines to establish the block indentation level (the first non-empty line)
  while (l->current_pos < l->source_len) {
    size_t line_pos = l->current_pos;
    int line_indent = 0;
    
    // Count spaces at start of the line to check nesting level
    while (l->current_pos < l->source_len && l->source[l->current_pos] == ' ') {
      line_indent++;
      l->current_pos++;
    }
    
    // Check if the current line contains only spaces, comments, or a newline
    bool is_empty = (l->current_pos >= l->source_len || 
                     is_newline(l->source[l->current_pos]) || 
                     l->source[l->current_pos] == '#');
                     
    if (is_empty) {
      // Consume the entire empty/comment line and proceed to the next line
      if (l->current_pos < l->source_len && l->source[l->current_pos] == '#') {
        while (l->current_pos < l->source_len && !is_newline(l->source[l->current_pos])) {
          l->current_pos++;
        }
      }
      if (l->current_pos < l->source_len && is_newline(l->source[l->current_pos])) {
        char nl = l->source[l->current_pos];
        l->current_pos++;
        if (nl == '\r' && l->current_pos < l->source_len && l->source[l->current_pos] == '\n') {
          l->current_pos++;
        }
      }
      l->current_line++;
      l->current_col = 0;
      continue;
    }
    
    // Non-empty line: establish block indentation level
    block_indent = line_indent;
    if (block_indent <= parent_indent) {
      // Indentation is not deeper than parent context; block is empty
      l->current_pos = line_pos;
      l->is_line_start = true;
      l->on_new_line = true;
      return (Token){.value = {.string = {l->source + line_pos, 0}}, T_STRING};
    }
    
    // Block content starts right after the block indentation spaces
    block_start = l->current_pos;
    
    // Consume the rest of the first content line
    while (l->current_pos < l->source_len && !is_newline(l->source[l->current_pos])) {
      l->current_pos++;
    }
    if (l->current_pos < l->source_len) {
      char nl = l->source[l->current_pos];
      l->current_pos++;
      if (nl == '\r' && l->current_pos < l->source_len && l->source[l->current_pos] == '\n') {
        l->current_pos++;
      }
    }
    l->current_line++;
    l->current_col = 0;
    break;
  }
  
  if (block_indent == -1) {
    // Reached EOF without any non-empty content lines
    l->is_line_start = true;
    l->on_new_line = true;
    return (Token){.value = {.string = {l->source + l->source_len, 0}}, T_STRING};
  }
  
  // Loop to consume subsequent lines belonging to the block scalar
  while (l->current_pos < l->source_len) {
    size_t line_pos = l->current_pos;
    int line_indent = 0;
    
    // Count leading indentation spaces
    while (l->current_pos < l->source_len && l->source[l->current_pos] == ' ') {
      line_indent++;
      l->current_pos++;
    }
    
    bool is_empty = (l->current_pos >= l->source_len || 
                     is_newline(l->source[l->current_pos]) || 
                     l->source[l->current_pos] == '#');
                     
    if (is_empty) {
      // Empty lines inside a block scalar are preserved as part of the block
      if (l->current_pos < l->source_len && l->source[l->current_pos] == '#') {
        while (l->current_pos < l->source_len && !is_newline(l->source[l->current_pos])) {
          l->current_pos++;
        }
      }
      if (l->current_pos < l->source_len && is_newline(l->source[l->current_pos])) {
        char nl = l->source[l->current_pos];
        l->current_pos++;
        if (nl == '\r' && l->current_pos < l->source_len && l->source[l->current_pos] == '\n') {
          l->current_pos++;
        }
      }
      l->current_line++;
      l->current_col = 0;
      continue;
    }
    
    // Check if the non-empty line's indentation belongs to this block
    if (line_indent >= block_indent) {
      // Consume the line content
      while (l->current_pos < l->source_len && !is_newline(l->source[l->current_pos])) {
        l->current_pos++;
      }
      if (l->current_pos < l->source_len) {
        char nl = l->source[l->current_pos];
        l->current_pos++;
        if (nl == '\r' && l->current_pos < l->source_len && l->source[l->current_pos] == '\n') {
          l->current_pos++;
        }
      }
      l->current_line++;
      l->current_col = 0;
    } else {
      // Indentation is less than block_indent: terminates the block scalar.
      // Rewind to the start of this line so it can be scanned normally in the next iteration.
      block_end = line_pos;
      l->current_pos = line_pos;
      l->is_line_start = true;
      l->on_new_line = true;
      return (Token){.value = {.string = {l->source + block_start, block_end - block_start}}, T_STRING};
    }
  }
  
  // Reached EOF: terminate the block at the end of the source buffer
  block_end = l->source_len;
  l->is_line_start = true;
  l->on_new_line = true;
  return (Token){.value = {.string = {l->source + block_start, block_end - block_start}}, T_STRING};
  /*#endregion*/
}

void yaml_lexer_init(T *l, const unsigned char *source, size_t len) {
  /*#region*/
  assert(l);
  assert(source);
  l->source = source;
  l->source_len = len;
  l->current_pos = 0;
  if (len >= 3 && source[0] == 0xEF && source[1] == 0xBB && source[2] == 0xBF) {
    l->current_pos = 3;
  }
  
  l->indent_stack[0] = 0;
  l->indent_top = 0;
  l->pending_dedents = 0;
  l->emit_indent = false;
  
  l->is_line_start = true;
  l->current_line = 1;
  l->current_col = 0;
  l->on_new_line = true;
  
  l->has_queued_token = false;
  l->flow_depth = 0;
  /*#endregion*/
}



static Token yaml_lexer_next_token_internal(T *l) {
  /*#region*/
  // 1. Return any queued virtual dedent/indent tokens first
  if (l->pending_dedents > 0) {
    l->pending_dedents--;
    return (Token){.value = {.string = {NULL, 0}}, T_YAML_DEDENT};
  }
  if (l->emit_indent) {
    l->emit_indent = false;
    return (Token){.value = {.string = {NULL, 0}}, T_YAML_INDENT};
  }
  if (l->has_queued_token) {
    l->has_queued_token = false;
    return l->queued_token;
  }

  // 2. Line start handling (calculates indentation levels)
  if (l->is_line_start) {
    l->is_line_start = false;

    if (l->flow_depth > 0) {
      while (l->current_pos < l->source_len) {
        while (l->current_pos < l->source_len && (l->source[l->current_pos] == ' ' || l->source[l->current_pos] == '\t')) {
          l->current_pos++;
        }
        if (l->current_pos >= l->source_len) break;
        char c = l->source[l->current_pos];
        if (is_newline(c) || c == '#') {
          while (l->current_pos < l->source_len && !is_newline(l->source[l->current_pos])) {
            l->current_pos++;
          }
          if (l->current_pos < l->source_len) {
            char nl = l->source[l->current_pos];
            l->current_pos++;
            if (nl == '\r' && l->current_pos < l->source_len && l->source[l->current_pos] == '\n') {
              l->current_pos++;
            }
          }
          l->current_line++;
          l->current_col = 0;
          continue;
        }
        break;
      }
    } else {
      // Scan lines until we find a non-empty, non-comment line
      while (l->current_pos < l->source_len) {
        size_t line_start_pos = l->current_pos;
        int indent = 0;

      // Count leading spaces
      while (l->current_pos < l->source_len) {
        char c = l->source[l->current_pos];
        if (c == ' ') {
          indent++;
          l->current_pos++;
        } else if (c == '\t') {
          // Tabs are not allowed for indentation in YAML
          return (Token){.value = {.string = {l->source + l->current_pos, 1}}, T_ERROR};
        } else {
          break;
        }
      }

      // Check if it's an empty line or comment line
      if (l->current_pos >= l->source_len) {
        break; // EOF
      }
      char c = l->source[l->current_pos];
      if (is_newline(c) || c == '#') {
        // Skip rest of the line
        while (l->current_pos < l->source_len && !is_newline(l->source[l->current_pos])) {
          l->current_pos++;
        }
        if (l->current_pos < l->source_len) {
          char nl = l->source[l->current_pos];
          l->current_pos++; // Skip newline char
          if (nl == '\r' && l->current_pos < l->source_len && l->source[l->current_pos] == '\n') {
            l->current_pos++; // Skip CRLF
          }
        }
        l->current_line++;
        l->current_col = 0;
        // Keep looping to next line
        continue;
      }

      // We found a real content line!
      l->current_col = indent;
      int prev_indent = l->indent_stack[l->indent_top];

      if (indent > prev_indent) {
        if (l->indent_top + 1 >= MAX_YAML_DEPTH) {
          return (Token){.value = {.string = {NULL, 0}}, T_ERROR};
        }
        l->indent_stack[++l->indent_top] = indent;
        l->emit_indent = true;
        
        // Rewind to line start so we process the content on next iteration,
        // but set is_line_start to false so we don't recalculate indentation.
        l->current_pos = line_start_pos;
        l->current_col = 0;
        l->emit_indent = false;
        return (Token){.value = {.string = {NULL, 0}}, T_YAML_INDENT};
      } 
      else if (indent < prev_indent) {
        // Find if this indent matches a previous level
        int matching_idx = -1;
        for (int i = 0; i <= l->indent_top; i++) {
          if (l->indent_stack[i] == indent) {
            matching_idx = i;
            break;
          }
        }
        if (matching_idx == -1) {
          return (Token){.value = {.string = {l->source + l->current_pos, 1}}, T_ERROR};
        }
        l->pending_dedents = l->indent_top - matching_idx;
        l->indent_top = matching_idx;
        
        // Defer parsing by rewinding
        l->current_pos = line_start_pos;
        l->current_col = 0;
        l->pending_dedents--; // Yield one immediately
        return (Token){.value = {.string = {NULL, 0}}, T_YAML_DEDENT};
      }
      
      // indent == prev_indent: no change in nesting
      break;
    }
  }
  }

  // 3. Skip inline spaces
  skip_spaces_on_line(l);

  // 4. Check EOF
  if (l->current_pos >= l->source_len) {
    if (l->indent_top > 0) {
      l->pending_dedents = l->indent_top;
      l->indent_top = 0;
      l->pending_dedents--; // Yield one immediately
      return (Token){.value = {.string = {NULL, 0}}, T_YAML_DEDENT};
    }
    return (Token){.value = {.string = {NULL, 0}}, T_EOF};
  }

  const unsigned char *token_start = l->source + l->current_pos;
  char c = *token_start;

  // 5. Handle comments and newlines
  if (c == '#') {
    while (l->current_pos < l->source_len && !is_newline(l->source[l->current_pos])) {
      l->current_pos++;
    }
    l->is_line_start = true;
    l->on_new_line = true;
    l->current_line++;
    l->current_col = 0;
    return yaml_lexer_next_token_internal(l);
  }

  if (is_newline(c)) {
    l->current_pos++;
    if (c == '\r' && l->current_pos < l->source_len && l->source[l->current_pos] == '\n') {
      l->current_pos++;
    }
    l->is_line_start = true;
    l->on_new_line = true;
    l->current_line++;
    l->current_col = 0;
    return yaml_lexer_next_token_internal(l);
  }

  // 6. YAML Bullet / Sequence Item
  if (c == '-') {
    if (l->current_pos + 1 >= l->source_len ||
        l->source[l->current_pos + 1] == ' ' ||
        l->source[l->current_pos + 1] == '\t' ||
        is_newline(l->source[l->current_pos + 1])) {
      l->current_pos++;
      l->current_col++;
      return (Token){.value = {.string = {token_start, 1}}, T_YAML_BULLET};
    }
  }

  // 7. Structural Flow Tokens
  switch (c) {
    case '{': l->current_pos++; l->current_col++; l->flow_depth++; return (Token){.value = {.string = {token_start, 1}}, T_BRACE_OPEN};
    case '}': l->current_pos++; l->current_col++; if (l->flow_depth > 0) l->flow_depth--; return (Token){.value = {.string = {token_start, 1}}, T_BRACE_CLOSE};
    case '[': l->current_pos++; l->current_col++; l->flow_depth++; return (Token){.value = {.string = {token_start, 1}}, T_BRACKET_OPEN};
    case ']': l->current_pos++; l->current_col++; if (l->flow_depth > 0) l->flow_depth--; return (Token){.value = {.string = {token_start, 1}}, T_BRACKET_CLOSE};
    case ',': l->current_pos++; l->current_col++; return (Token){.value = {.string = {token_start, 1}}, T_COMMA};
    case ':':
      if (l->current_pos + 1 >= l->source_len ||
          l->source[l->current_pos + 1] == ' ' ||
          l->source[l->current_pos + 1] == '\t' ||
          is_newline(l->source[l->current_pos + 1])) {
        l->current_pos++;
        l->current_col++;
        return (Token){.value = {.string = {token_start, 1}}, T_COLON};
      }
      break;
  }

  // 8. Quoted Strings
  if (c == '"' || c == '\'') {
    return parse_quoted_string(l, c);
  }

  // 8b. Block Scalars
  if (c == '|' || c == '>') {
    return parse_block_scalar(l, c);
  }

  // 9. Unquoted Scalar
  return parse_unquoted_scalar(l);
  /*#endregion*/
}

Token yaml_lexer_next_token(T *l) {
  if (l->is_line_start) {
    l->on_new_line = true;
  }
  bool start_on_new_line = l->on_new_line;
  Token tok = yaml_lexer_next_token_internal(l);
  if (tok.type == T_STRING && l->is_line_start) {
    tok.on_new_line = start_on_new_line;
  } else {
    tok.on_new_line = l->on_new_line;
  }
  if (tok.type != T_YAML_INDENT && tok.type != T_YAML_DEDENT) {
    l->on_new_line = false;
  }
  return tok;
}
#endif // JSONV_YAML_SUPPORT
