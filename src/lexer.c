#include "lexer.h"
#include "assert.h"
#include "token.h"
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#define T Lexer

static void skip_whitespace(T *l);
static Token lex_string(T *l, const unsigned char *token_start);
static Token lex_literal(T *l, const unsigned char *token_start);
static Token lex_number(T *l, const unsigned char *token_start);

typedef struct T {
  const unsigned char *source; // The entire JSON string input
  size_t source_len;
  size_t current_pos;
} T;

static void skip_whitespace(T *l) {
  /*#region*/
  while (l->current_pos < l->source_len) {
    char c = l->source[l->current_pos];
    if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
      l->current_pos++;
    } else {
      break;
    }
  }
  /*#endregion*/
}

static int is_hex_digit(char c) {
  /*#region*/
  return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') ||
         (c >= 'A' && c <= 'F');
  /*#endregion*/
}

static Token lex_string(Lexer *l, const unsigned char *token_start) {
  /*#region*/
  // We already consumed the opening '"'
  size_t start_pos = l->current_pos;

  while (l->current_pos < l->source_len) {
    // 1. Cast to unsigned char to handle UTF-8/Emojis correctly
    unsigned char c = (unsigned char)l->source[l->current_pos];

    if (c == '"') {
      l->current_pos++; // Consume closing quote
      return (Token){T_STRING, .string = {token_start + 1,
                                          l->current_pos - start_pos - 1}};
    }

    if (c == '\\') {
      l->current_pos++; // Consume '\'
      if (l->current_pos < l->source_len) {
        char escaped_char = l->source[l->current_pos];

        // Handle standard escapes
        if (strchr("\"\\/bfnrt", escaped_char) != NULL) {
          l->current_pos++;
          continue;
        }

        // 2. Handle Unicode escapes (\uXXXX)
        // Emojis can be surrogate pairs (e.g. \uD83D\uDE00)
        if (escaped_char == 'u') {
          l->current_pos++; // Consume 'u'
          // We expect 4 hex digits
          for (int i = 0; i < 4; i++) {
            if (l->current_pos < l->source_len &&
                is_hex_digit(l->source[l->current_pos])) {
              l->current_pos++;
            } else {
              return (Token){T_ERROR, .string = {token_start,
                                                 l->current_pos - (token_start -
                                                                   l->source)}};
            }
          }
          continue;
        }
      }

      return (Token){
          T_ERROR,
          .string = {token_start, l->current_pos - (token_start - l->source)}};
    }

    // 3. Check for control characters using the unsigned value
    // Emojis (e.g., 0xF0) are now > 32, so they pass this check.
    if (c < 32) {
      return (Token){
          T_ERROR,
          .string = {token_start, l->current_pos - (token_start - l->source)}};
    }

    l->current_pos++;
  }

  return (Token){T_ERROR, .string = {token_start, l->source_len - (token_start -
                                                                   l->source)}};
  /*#endregion*/
}

static Token lex_literal(T *l, const unsigned char *token_start) {
  /*#region*/
  // We have already consumed the first char (t, f, or n)

  if (token_start[0] == 't' && l->current_pos + 3 <= l->source_len &&
      strncmp((char*)l->source + l->current_pos, "rue", 3) == 0) {
    l->current_pos += 3;
    return (Token){T_TRUE, {0}}; // "true"
  }

  if (token_start[0] == 'f' && l->current_pos + 4 <= l->source_len &&
      strncmp((char*)l->source + l->current_pos, "alse", 4) == 0) {
    l->current_pos += 4;
    return (Token){T_FALSE, {0}}; // "false"
  }

  if (token_start[0] == 'n' && l->current_pos + 3 <= l->source_len &&
      strncmp((char*)l->source + l->current_pos, "ull", 3) == 0) {
    l->current_pos += 3;
    return (Token){T_NULL, {0}}; // "null"
  }

  // If it started with t, f, or n but wasn't a recognized literal
  return (Token){T_ERROR, .string = {token_start, 1}};
  /*#endregion*/
}

/*
 * Checks if a character is a decimal digit (0-9).
 */
static bool is_digit(char c) {
  /*#region*/
  return c >= '0' && c <= '9';
  /*#endregion*/
}

/*
 * Checks if a character is a digit or a sign (+ or -).
 */
// static bool is_digit_or_sign(char c) {
//   /*#region*/
//   return is_digit(c) || c == '+' || c == '-';
//   /*#endregion*/
// }

/*
 * Helper to advance the lexer position if the current character matches the
 * expected character.
 */
static bool advance_if_match(Lexer *l, char expected) {
  /*#region*/
  if (l->current_pos < l->source_len && l->source[l->current_pos] == expected) {
    l->current_pos++;
    return true;
  }
  return false;
  /*#endregion*/
}

static Token lex_number(Lexer *l, const unsigned char *token_start) {
  size_t start_pos = l->current_pos - 1;

  // --- 1. Integer Part ---
  if (token_start[0] == '-') {
    if (!is_digit(l->source[l->current_pos])) {
      return (Token){T_ERROR, .string = {token_start, 1}};
    }
  }

  if (token_start[0] == '0' && l->current_pos < l->source_len) {
    if (is_digit(l->source[l->current_pos])) {
      return (Token){T_ERROR, .string = {token_start, 2}};
    }
  }

  while (l->current_pos < l->source_len &&
         is_digit(l->source[l->current_pos])) {
    l->current_pos++;
  }

  // --- 2. Fractional Part ---
  if (l->current_pos < l->source_len && l->source[l->current_pos] == '.') {
    l->current_pos++;
    if (l->current_pos >= l->source_len ||
        !is_digit(l->source[l->current_pos])) {
      return (Token){T_ERROR,
                     .string = {token_start, l->current_pos - start_pos}};
    }
    while (l->current_pos < l->source_len &&
           is_digit(l->source[l->current_pos])) {
      l->current_pos++;
    }
  }

  // --- 3. Exponent Part ---
  char c = l->source[l->current_pos];
  if (c == 'e' || c == 'E') {
    l->current_pos++;
    if (advance_if_match(l, '+') || advance_if_match(l, '-')) {
    }

    if (l->current_pos >= l->source_len ||
        !is_digit(l->source[l->current_pos])) {
      return (Token){T_ERROR,
                     .string = {token_start, l->current_pos - start_pos}};
    }
    while (l->current_pos < l->source_len &&
           is_digit(l->source[l->current_pos])) {
      l->current_pos++;
    }
  }

  // --- 4. Parse Value with Overflow Check ---
  size_t length = l->current_pos - start_pos;
  double value = 0.0;
  char buffer[128];

  if (length < sizeof(buffer)) {
    memcpy(buffer, token_start, length);
    buffer[length] = '\0';

    char *endptr;
    errno = 0; // Reset errno before calling strtod
    value = strtod(buffer, &endptr);

    // Check 1: Did parsing happen?
    if (endptr == buffer) {
      value = 0.0;
    }
    // Check 2: Overflow or Underflow?
    else if (errno == ERANGE) {
      // errno is set to ERANGE if the value is too large (infinity)
      // or too small (underflow).
      value = 0.0;
    }
  } else {
    // Error: Number string too long for buffer
    value = 0.0;
  }

  return (Token){T_NUMBER, .number = value};
}

Token lexer_next_token(T *l) {
  /*#region*/
  skip_whitespace(l);

  if (l->current_pos >= l->source_len) {
    return (Token){T_EOF, {0}};
  }

  const unsigned char *token_start = l->source + l->current_pos;
  char c = *token_start;
  l->current_pos++; // Advance one character initially

  switch (c) {
  // Structural Tokens
  case '{':
    return (Token){T_BRACE_OPEN, .string = {token_start, 1}};
  case '}':
    return (Token){T_BRACE_CLOSE, .string = {token_start, 1}};
  case '[':
    return (Token){T_BRACKET_OPEN, .string = {token_start, 1}};
  case ']':
    return (Token){T_BRACKET_CLOSE, .string = {token_start, 1}};
  case ':':
    return (Token){T_COLON, .string = {token_start, 1}};
  case ',':
    return (Token){T_COMMA, .string = {token_start, 1}};

  // Literal Tokens
  case '"':
    return lex_string(l, token_start);

  // Numbers and Literals (true, false, null)
  case '-': // Numbers can start with '-'
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
    return lex_number(l, token_start);

  case 't': // Could be 'true'
  case 'f': // Could be 'false'
  case 'n': // Could be 'null'
    return lex_literal(l, token_start);

  default:
    return (Token){T_ERROR, .string = {token_start, 1}}; // Unexpected character
  }
  /*#endregion*/
}

void lexer_init(T **l, const unsigned char *source, size_t len) {
  /*#region*/
  assert(*l);
  assert(source);
  assert(len);
  Lexer *self = *l;
  self->source = source;
  self->source_len = len;
  self->current_pos = 0;
  /*#endregion*/
}

size_t lexer_sizeof(void) {
  /*#region*/
  return sizeof(T);
  /*#endregion*/
}

void lexer_free(T **l) {
  /*#region*/
  T *lexer = *l;
  free(lexer);
  l = NULL;
  /*#endregion*/
}
