#include "lexer.h"
#include "assert.h"
#include "token.h"
#include <errno.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#define T Lexer

static void skip_whitespace(T *l);
static bool advance_if_match(Lexer *l, char expected);
static int is_hex_digit(char c);
static bool is_digit(char c);
static Token parse_string(T *l, const unsigned char *token_start);
static Token parse_literal(T *l, const unsigned char *token_start);
static Token parse_number(T *l, const unsigned char *token_start);

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

static bool is_digit(char c) {
  /*#region*/
  return c >= '0' && c <= '9';
  /*#endregion*/
}

static Token parse_string(Lexer *l, const unsigned char *token_start) {
  /*#region*/
  // We already consumed the opening '"'
  size_t start_pos = l->current_pos;

  while (l->current_pos < l->source_len) {
    // 1. Cast to unsigned char to handle UTF-8/Emojis correctly
    unsigned char c = (unsigned char)l->source[l->current_pos];

    if (c == '"') {
      l->current_pos++; // Consume closing quote
      return (Token){
          .value = {.string = {token_start + 1,
                               l->current_pos - start_pos - 1}},
          T_STRING,
      };
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
              return (Token){
                  .value = {.string = {token_start,
                                       l->current_pos -
                                           (token_start - l->source)}},
                  T_ERROR,
              };
            }
          }
          continue;
        }
      }

      return (Token){
          .value = {.string = {token_start,
                               l->current_pos - (token_start - l->source)}},
          T_ERROR,
      };
    }

    // 3. Check for control characters using the unsigned value
    // Emojis (e.g., 0xF0) are now > 32, so they pass this check.
    if (c < 32) {
      return (Token){
          .value = {.string = {token_start,
                               l->current_pos - (token_start - l->source)}},
          T_ERROR};
    }

    l->current_pos++;
  }

  return (Token){
      .value = {.string = {token_start,
                           l->source_len - (token_start - l->source)}},
      T_ERROR,
  };
  /*#endregion*/
}

static Token parse_literal(T *l, const unsigned char *token_start) {
  /*#region*/
  // We have already consumed the first char (t, f, or n)

  if (token_start[0] == 't' && l->current_pos + 3 <= l->source_len &&
      strncmp((char *)l->source + l->current_pos, "rue", 3) == 0) {
    l->current_pos += 3;
    return (Token){
        {0},
        T_TRUE,
    }; // "true"
  }

  if (token_start[0] == 'f' && l->current_pos + 4 <= l->source_len &&
      strncmp((char *)l->source + l->current_pos, "alse", 4) == 0) {
    l->current_pos += 4;
    return (Token){{0}, T_FALSE}; // "false"
  }

  if (token_start[0] == 'n' && l->current_pos + 3 <= l->source_len &&
      strncmp((char *)l->source + l->current_pos, "ull", 3) == 0) {
    l->current_pos += 3;
    return (Token){{0}, T_NULL}; // "null"
  }

  // If it started with t, f, or n but wasn't a recognized literal
  return (Token){
      .value = {.string = {token_start, 1}},
      T_ERROR,
  };
  /*#endregion*/
}

static Token parse_number(Lexer *l, const unsigned char *token_start) {
  /*#region*/
  size_t start_pos = l->current_pos - 1;

  // --- 1. Handle Start Conditions ---

  // CASE A: Number starts with '.' (e.g., ".2")
  if (token_start[0] == '.') {
    // If it starts with '.', it MUST be followed by a digit.
    // (A standalone '.' is usually a different token, not a number)
    if (l->current_pos >= l->source_len ||
        !is_digit(l->source[l->current_pos])) {
      return (Token){
          .value = {.string = {(const unsigned char *)token_start, 1}},
          T_ERROR};
    }

    // Consume the digits (these are the fractional part)
    while (l->current_pos < l->source_len &&
           is_digit(l->source[l->current_pos])) {
      l->current_pos++;
    }
  }
  // CASE B: Number starts with Digit, '+', or '-'
  else {
    // 1a. Sign Check
    if (token_start[0] == '-' || token_start[0] == '+') {
      if (!is_digit(l->source[l->current_pos])) {
        return (Token){
            .value = {.string = {(const unsigned char *)token_start, 1}},
            T_ERROR};
      }
    }

    // 1b. Integer Part
    while (l->current_pos < l->source_len &&
           is_digit(l->source[l->current_pos])) {
      l->current_pos++;
    }

    // 1c. Fractional Part (Optional)
    if (l->current_pos < l->source_len && l->source[l->current_pos] == '.') {
      l->current_pos++; // Consume '.'
      if (l->current_pos >= l->source_len ||
          !is_digit(l->source[l->current_pos])) {
        return (Token){.value = {.string = {(const unsigned char *)token_start,
                                            l->current_pos - start_pos}},
                       T_ERROR};
      }
      while (l->current_pos < l->source_len &&
             is_digit(l->source[l->current_pos])) {
        l->current_pos++;
      }
    }
  }

  // --- 2. Exponent Part (Shared) ---
  // Works for both "1.2e5" and ".2e5"
  char c = l->source[l->current_pos];
  if (c == 'e' || c == 'E') {
    l->current_pos++;
    if (advance_if_match(l, '+') || advance_if_match(l, '-')) {
    }

    if (l->current_pos >= l->source_len ||
        !is_digit(l->source[l->current_pos])) {
      return (Token){
          .value = {.string = {(const unsigned char *)token_start,
                               l->current_pos - start_pos}},
          T_ERROR,
      };
    }
    while (l->current_pos < l->source_len &&
           is_digit(l->source[l->current_pos])) {
      l->current_pos++;
    }
  }

  // --- 3. Parse Value ---
  size_t length = l->current_pos - start_pos;
  double value = 0.0;
  char buffer[128];

  if (length < sizeof(buffer)) {
    memcpy(buffer, token_start, length);
    buffer[length] = '\0';

    char *endptr;
    errno = 0;
    value = strtod(buffer, &endptr);

    if (endptr == buffer) {
      value = 0.0;
    } else if (errno == ERANGE) {
      value = 0.0;
    }
  } else {
    value = 0.0;
  }

  return (Token){
      .value = {.number = value},
      T_NUMBER,
  };
  /*#endregion*/
}

Token lexer_next_token(T *l) {
  /*#region*/
  skip_whitespace(l);

  if (l->current_pos >= l->source_len) {
    return (Token){
        {0},
        T_EOF,
    };
  }

  const unsigned char *token_start = l->source + l->current_pos;
  char c = *token_start;
  l->current_pos++; // Advance one character initially

  switch (c) {
  // Structural Tokens
  case '{':
    return (Token){.value = {.string = {token_start, 1}}, T_BRACE_OPEN};
  case '}':
    return (Token){.value = {.string = {token_start, 1}}, T_BRACE_CLOSE};
  case '[':
    return (Token){.value = {.string = {token_start, 1}}, T_BRACKET_OPEN};
  case ']':
    return (Token){.value = {.string = {token_start, 1}}, T_BRACKET_CLOSE};
  case ':':
    return (Token){.value = {.string = {token_start, 1}}, T_COLON};
  case ',':
    return (Token){.value = {.string = {token_start, 1}}, T_COMMA};

  // Literal Tokens
  case '"':
    return parse_string(l, token_start);

  // Numbers and Literals (true, false, null)
  case '-': // Numbers can start with '-'
  case '+': // Numbers can start with '+'
  case '.': // Numbers can start with '+'
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

  case 't': // Could be 'true'
  case 'f': // Could be 'false'
  case 'n': // Could be 'null'
    return parse_literal(l, token_start);

  default:
    return (Token){.value = {.string = {token_start, 1}},
                   T_ERROR}; // Unexpected character
  }
  /*#endregion*/
}

void lexer_init(T *l, const unsigned char *source, size_t len) {
  /*#region*/
  assert(l);
  assert(source);
  assert(len);
  l->source = source;
  l->source_len = len;
  l->current_pos = 0;
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
