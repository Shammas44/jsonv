#include "lexer.h"
#include "assert.h"
#include "token.h"
#include "mem.h" // Conforms to strict allocation rules
#include <errno.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#define T Lexer

static void skip_whitespace(T *l);
static int is_hex_digit(char c);
static bool is_digit(char c);
static Token parse_string(T *l, const unsigned char *token_start);
static Token parse_literal(T *l, const unsigned char *token_start);
static Token parse_number(T *l, const unsigned char *token_start);



static void skip_whitespace(T *l) {
  /*#region*/
  // Optimization: Fast Lookup Table (LUT) for branchless whitespace scanning
  static const uint8_t is_space_lut[256] = {
      [' '] = 1, ['\t'] = 1, ['\r'] = 1, ['\n'] = 1
  };
  while (l->current_pos < l->source_len && is_space_lut[l->source[l->current_pos]]) {
    l->current_pos++;
  }
  /*#endregion*/
}

static int is_hex_digit(char c) {
  /*#region*/
  return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') ||
         (c >= 'A' && c <= 'F');
  /*#endregion*/
}

static bool is_digit(char c) {
  /*#region*/
  return c >= '0' && c <= '9';
  /*#endregion*/
}

static Token parse_string(Lexer *l, const unsigned char *token_start) {
  /*#region*/
  size_t start_pos = l->current_pos;
  bool has_escape = false;

  while (l->current_pos < l->source_len) {
    unsigned char c = l->source[l->current_pos];

    if (c == '"') {
      l->current_pos++; // Consume closing quote
      return (Token){.value = {.string = {token_start + 1,
                                          l->current_pos - start_pos - 1}},
                     .type = T_STRING,
                     .has_escape = has_escape};
    }

    if (c == '\\') {
      has_escape = true;
      l->current_pos++; // Consume '\'
      if (l->current_pos < l->source_len) {
        char escaped_char = l->source[l->current_pos];

        if (escaped_char == '"'  || escaped_char == '\\' ||
            escaped_char == '/'  || escaped_char == 'b'  ||
            escaped_char == 'f'  || escaped_char == 'n'  ||
            escaped_char == 't'  || escaped_char == 'r') {
          l->current_pos++;
          continue;
        }

        if (escaped_char == 'u') {
          l->current_pos++; // Consume 'u'
          uint16_t u1 = 0;
          for (int i = 0; i < 4; i++) {
            if (l->current_pos < l->source_len && is_hex_digit(l->source[l->current_pos])) {
              char ch = l->source[l->current_pos++];
              u1 = (u1 << 4) | (ch >= '0' && ch <= '9' ? ch - '0' :
                                ch >= 'a' && ch <= 'f' ? ch - 'a' + 10 :
                                ch - 'A' + 10);
            } else {
              return (Token){.value = {.string = {token_start, l->current_pos - (token_start - l->source)}}, .type = T_ERROR};
            }
          }
          if (u1 >= 0xD800 && u1 <= 0xDBFF) {
            if (l->current_pos + 6 <= l->source_len &&
                l->source[l->current_pos] == '\\' &&
                l->source[l->current_pos + 1] == 'u') {
              uint16_t u2 = 0;
              bool valid_low = true;
              for (int i = 0; i < 4; i++) {
                char ch = l->source[l->current_pos + 2 + i];
                if (is_hex_digit(ch)) {
                  u2 = (u2 << 4) | (ch >= '0' && ch <= '9' ? ch - '0' :
                                    ch >= 'a' && ch <= 'f' ? ch - 'a' + 10 :
                                    ch - 'A' + 10);
                } else {
                  valid_low = false;
                  break;
                }
              }
              if (valid_low && u2 >= 0xDC00 && u2 <= 0xDFFF) {
                l->current_pos += 6;
                continue;
              }
            }
            return (Token){.value = {.string = {token_start, l->current_pos - (token_start - l->source)}}, .type = T_ERROR};
          } else if (u1 >= 0xDC00 && u1 <= 0xDFFF) {
            return (Token){.value = {.string = {token_start, l->current_pos - (token_start - l->source)}}, .type = T_ERROR};
          }
          continue;
        }
      }

      return (Token){
          .value = {.string = {token_start,
                               l->current_pos - (token_start - l->source)}},
          .type = T_ERROR,
      };
    }

    if (c < 0x20) {
      return (Token){.value = {.string = {token_start, l->current_pos - (token_start - l->source)}}, .type = T_ERROR};
    } else if (c <= 0x7F) {
      l->current_pos++;
    } else if (c >= 0xC2 && c <= 0xDF) {
      if (l->current_pos + 1 >= l->source_len) return (Token){.value = {.string = {token_start, l->current_pos - (token_start - l->source)}}, .type = T_ERROR};
      unsigned char b2 = l->source[l->current_pos + 1];
      if (b2 < 0x80 || b2 > 0xBF) return (Token){.value = {.string = {token_start, l->current_pos - (token_start - l->source)}}, .type = T_ERROR};
      l->current_pos += 2;
    } else if (c >= 0xE0 && c <= 0xEF) {
      if (l->current_pos + 2 >= l->source_len) return (Token){.value = {.string = {token_start, l->current_pos - (token_start - l->source)}}, .type = T_ERROR};
      unsigned char b2 = l->source[l->current_pos + 1];
      unsigned char b3 = l->source[l->current_pos + 2];
      if (c == 0xE0 && (b2 < 0xA0 || b2 > 0xBF)) return (Token){.value = {.string = {token_start, l->current_pos - (token_start - l->source)}}, .type = T_ERROR};
      if (c == 0xED && (b2 < 0x80 || b2 > 0x9F)) return (Token){.value = {.string = {token_start, l->current_pos - (token_start - l->source)}}, .type = T_ERROR};
      if (c != 0xE0 && c != 0xED && (b2 < 0x80 || b2 > 0xBF)) return (Token){.value = {.string = {token_start, l->current_pos - (token_start - l->source)}}, .type = T_ERROR};
      if (b3 < 0x80 || b3 > 0xBF) return (Token){.value = {.string = {token_start, l->current_pos - (token_start - l->source)}}, .type = T_ERROR};
      l->current_pos += 3;
    } else if (c >= 0xF0 && c <= 0xF4) {
      if (l->current_pos + 3 >= l->source_len) return (Token){.value = {.string = {token_start, l->current_pos - (token_start - l->source)}}, .type = T_ERROR};
      unsigned char b2 = l->source[l->current_pos + 1];
      unsigned char b3 = l->source[l->current_pos + 2];
      unsigned char b4 = l->source[l->current_pos + 3];
      if (c == 0xF0 && (b2 < 0x90 || b2 > 0xBF)) return (Token){.value = {.string = {token_start, l->current_pos - (token_start - l->source)}}, .type = T_ERROR};
      if (c == 0xF4 && (b2 < 0x80 || b2 > 0x8F)) return (Token){.value = {.string = {token_start, l->current_pos - (token_start - l->source)}}, .type = T_ERROR};
      if (c != 0xF0 && c != 0xF4 && (b2 < 0x80 || b2 > 0xBF)) return (Token){.value = {.string = {token_start, l->current_pos - (token_start - l->source)}}, .type = T_ERROR};
      if (b3 < 0x80 || b3 > 0xBF || b4 < 0x80 || b4 > 0xBF) return (Token){.value = {.string = {token_start, l->current_pos - (token_start - l->source)}}, .type = T_ERROR};
      l->current_pos += 4;
    } else {
      return (Token){.value = {.string = {token_start, l->current_pos - (token_start - l->source)}}, .type = T_ERROR};
    }
  }

  return (Token){
      .value = {.string = {token_start,
                           l->source_len - (token_start - l->source)}},
      .type = T_ERROR,
  };
  /*#endregion*/
}

static Token parse_literal(T *l, const unsigned char *token_start) {
  /*#region*/
  if (token_start[0] == 't' && l->current_pos + 3 <= l->source_len &&
      l->source[l->current_pos] == 'r' &&
      l->source[l->current_pos + 1] == 'u' &&
      l->source[l->current_pos + 2] == 'e') {
    l->current_pos += 3;
    return (Token){.type = T_TRUE};
  }

  if (token_start[0] == 'f' && l->current_pos + 4 <= l->source_len &&
      l->source[l->current_pos] == 'a' &&
      l->source[l->current_pos + 1] == 'l' &&
      l->source[l->current_pos + 2] == 's' &&
      l->source[l->current_pos + 3] == 'e') {
    l->current_pos += 4;
    return (Token){.type = T_FALSE};
  }

  if (token_start[0] == 'n' && l->current_pos + 3 <= l->source_len &&
      l->source[l->current_pos] == 'u' &&
      l->source[l->current_pos + 1] == 'l' &&
      l->source[l->current_pos + 2] == 'l') {
    l->current_pos += 3;
    return (Token){.type = T_NULL};
  }

  return (Token){.value = {.string = {token_start, 1}}, .type = T_ERROR};
  /*#endregion*/
}

static Token parse_number(Lexer *l, const unsigned char *token_start) {
  /*#region*/
  size_t start_pos = l->current_pos - 1;

  if (token_start[0] == '+' || token_start[0] == '.') {
    return (Token){
        .value = {.raw_number = {token_start, 1}},
        .type = T_ERROR};
  }

  if (token_start[0] == '-') {
    if (l->current_pos >= l->source_len || !is_digit(l->source[l->current_pos])) {
      return (Token){
          .value = {.raw_number = {token_start, l->current_pos - start_pos}},
          .type = T_ERROR};
    }
  }

  unsigned char first_digit = (token_start[0] == '-') ? l->source[l->current_pos++] : token_start[0];

  if (first_digit == '0') {
    if (l->current_pos < l->source_len && is_digit(l->source[l->current_pos])) {
      return (Token){
          .value = {.raw_number = {token_start, l->current_pos - start_pos}},
          .type = T_ERROR};
    }
  } else if (first_digit >= '1' && first_digit <= '9') {
    while (l->current_pos < l->source_len && is_digit(l->source[l->current_pos])) {
      l->current_pos++;
    }
  } else {
    return (Token){
        .value = {.raw_number = {token_start, l->current_pos - start_pos}},
        .type = T_ERROR};
  }

  if (l->current_pos < l->source_len && l->source[l->current_pos] == '.') {
    l->current_pos++;
    if (l->current_pos >= l->source_len || !is_digit(l->source[l->current_pos])) {
      return (Token){
          .value = {.raw_number = {token_start, l->current_pos - start_pos}},
          .type = T_ERROR};
    }
    while (l->current_pos < l->source_len && is_digit(l->source[l->current_pos])) {
      l->current_pos++;
    }
  }

  if (l->current_pos < l->source_len && (l->source[l->current_pos] == 'e' || l->source[l->current_pos] == 'E')) {
    l->current_pos++;
    if (l->current_pos < l->source_len && (l->source[l->current_pos] == '+' || l->source[l->current_pos] == '-')) {
      l->current_pos++;
    }
    if (l->current_pos >= l->source_len || !is_digit(l->source[l->current_pos])) {
      return (Token){
          .value = {.raw_number = {token_start, l->current_pos - start_pos}},
          .type = T_ERROR};
    }
    while (l->current_pos < l->source_len && is_digit(l->source[l->current_pos])) {
      l->current_pos++;
    }
  }

  size_t length = l->current_pos - start_pos;
  return (Token){.value = {.raw_number = {token_start, length}}, .type = T_NUMBER, .has_escape = false};
  /*#endregion*/
}

Token lexer_next_token(T *l) {
  /*#region*/
  skip_whitespace(l);

  if (l->current_pos >= l->source_len) {
    return (Token){{0}, T_EOF, false, false};
  }

  const unsigned char *token_start = l->source + l->current_pos;
  char c = *token_start;
  l->current_pos++;

  switch (c) {
  case '{':
    return (Token){.value = {.string = {token_start, 1}}, .type = T_BRACE_OPEN};
  case '}':
    return (Token){.value = {.string = {token_start, 1}}, .type = T_BRACE_CLOSE};
  case '[':
    return (Token){.value = {.string = {token_start, 1}}, .type = T_BRACKET_OPEN};
  case ']':
    return (Token){.value = {.string = {token_start, 1}}, .type = T_BRACKET_CLOSE};
  case ':':
    return (Token){.value = {.string = {token_start, 1}}, .type = T_COLON};
  case ',':
    return (Token){.value = {.string = {token_start, 1}}, .type = T_COMMA};

  case '"':
    return parse_string(l, token_start);

  case '-':
  case '+':
  case '.':
  case '0':
  case '1':
  case '2':
  case '3':
  case '4':
  case '5':
  case '6':
  case '7':
  case '8':
  case '9':
    return parse_number(l, token_start);

  case 't':
  case 'f':
  case 'n':
    return parse_literal(l, token_start);

  default:
    return (Token){.value = {.string = {token_start, 1}},
                   .type = T_ERROR};
  }
  /*#endregion*/
}

void lexer_init(T *l, const unsigned char *source, size_t len) {
  /*#region*/
  assert(l);
  assert(source);
  l->source = source;
  l->source_len = len;
  l->current_pos = 0;
  if (len >= 3 && source[0] == 0xEF && source[1] == 0xBB && source[2] == 0xBF) {
    l->current_pos = 3;
  }
  /*#endregion*/
}

size_t lexer_sizeof(void) {
  /*#region*/
  return sizeof(T);
  /*#endregion*/
}

void lexer_free(T **l) {
  /*#region*/
  // Fixed: Utilizes custom memory tracking FREE macro and zeroes caller's pointer safely
  if (l && *l) {
    FREE(*l);
  }
  /*#endregion*/
}
