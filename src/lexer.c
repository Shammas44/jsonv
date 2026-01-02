#include "lexer.h"
#include "assert.h"
#include "bitstack.h"
#include "mem.h"
#include "slidingwindow.h"
#include "token.h"
#include <stdlib.h>
#include <string.h>
#define T Lexer

typedef bool (*handler)(T *l);

static void skip_whitespace(T *l);
static Token lex_string(T *l, const char *token_start);
static Token lex_literal(T *l, const char *token_start);
static Token lex_number(T *l, const char *token_start);
static Token inner_next(T *l);

typedef struct T {
  const char *source; // The entire JSON string input
  size_t source_len;
  size_t current_pos;
  // Jsonv_BitStack *stack;
  Jsonv_SlidingWindow *window;
} T;

#define BRACE 0
#define BRACKET 1

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

static Token lex_string(T *l, const char *token_start) {
  /*#region*/
  // We already consumed the opening '"'
  size_t start_pos = l->current_pos;

  while (l->current_pos < l->source_len) {
    char c = l->source[l->current_pos];

    if (c == '"') {
      l->current_pos++; // Consume closing quote
      return (Token){T_STRING, token_start + 1, l->current_pos - start_pos - 1};
    }

    if (c == '\\') {
      // JSON supports escaped characters (\n, \t, \uXXXX, etc.)
      l->current_pos++; // Consume '\'
      if (l->current_pos < l->source_len) {
        char escaped_char = l->source[l->current_pos];
        if (strchr("\"\\/bfnrtu", escaped_char) != NULL) {
          l->current_pos++; // Consume the escaped character
          continue;
        }
      }
      // Error: Invalid escape sequence
      return (Token){T_ERROR, token_start,
                     l->current_pos - (token_start - l->source)};
    }

    // Check for control characters (must be escaped)
    if (c < 32) {
      return (Token){T_ERROR, token_start,
                     l->current_pos - (token_start - l->source)};
    }

    l->current_pos++;
  }

  // Error: Unterminated string (reached EOF)
  return (Token){T_ERROR, token_start,
                 l->source_len - (token_start - l->source)};
  /*#endregion*/
}

static Token lex_literal(T *l, const char *token_start) {
  /*#region*/
  // We have already consumed the first char (t, f, or n)

  if (token_start[0] == 't' && l->current_pos + 3 <= l->source_len &&
      strncmp(l->source + l->current_pos, "rue", 3) == 0) {
    l->current_pos += 3;
    return (Token){T_TRUE, token_start, 4}; // "true"
  }

  if (token_start[0] == 'f' && l->current_pos + 4 <= l->source_len &&
      strncmp(l->source + l->current_pos, "alse", 4) == 0) {
    l->current_pos += 4;
    return (Token){T_FALSE, token_start, 5}; // "false"
  }

  if (token_start[0] == 'n' && l->current_pos + 3 <= l->source_len &&
      strncmp(l->source + l->current_pos, "ull", 3) == 0) {
    l->current_pos += 3;
    return (Token){T_NULL, token_start, 4}; // "null"
  }

  // If it started with t, f, or n but wasn't a recognized literal
  return (Token){T_ERROR, token_start, 1};
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

static Token lex_number(Lexer *l, const char *token_start) {
  /*#region*/
  // We already consumed the first character (either '-', '0', or 1-9)
  // The current_pos now points to the second character (or later).
  size_t start_pos = l->current_pos - 1;

  // --- 1. Integer Part ---

  // If the number started with a minus sign ('-'), the next character must be a
  // digit.
  if (token_start[0] == '-') {
    if (!is_digit(l->source[l->current_pos])) {
      // Error: '-' must be followed by a digit.
      return (Token){T_ERROR, token_start, 1};
    }
  }

  // Check for leading zero, which must be followed by a decimal point or end of
  // number.
  if (token_start[0] == '0' && l->current_pos < l->source_len) {
    if (is_digit(l->source[l->current_pos])) {
      // Error: Leading zero not followed by a decimal point (e.g., 012)
      return (Token){T_ERROR, token_start, 2};
    }
  }

  // Consume all subsequent digits of the integer part (if not a single '0')
  while (l->current_pos < l->source_len &&
         is_digit(l->source[l->current_pos])) {
    l->current_pos++;
  }

  // --- 2. Fractional Part (Optional) ---
  if (l->current_pos < l->source_len && l->source[l->current_pos] == '.') {
    l->current_pos++; // Consume '.'

    // Must be followed by at least one digit
    if (l->current_pos >= l->source_len ||
        !is_digit(l->source[l->current_pos])) {
      // Error: '.' must be followed by a digit (e.g., 1.)
      return (Token){T_ERROR, token_start, l->current_pos - start_pos};
    }

    // Consume all subsequent digits of the fractional part
    while (l->current_pos < l->source_len &&
           is_digit(l->source[l->current_pos])) {
      l->current_pos++;
    }
  }

  // --- 3. Exponent Part (Optional) ---
  char c = l->source[l->current_pos];
  if (c == 'e' || c == 'E') {
    l->current_pos++; // Consume 'e' or 'E'

    // Optional sign (+ or -)
    if (advance_if_match(l, '+') || advance_if_match(l, '-')) {
      // Sign consumed
    }

    // Must be followed by at least one digit
    if (l->current_pos >= l->source_len ||
        !is_digit(l->source[l->current_pos])) {
      // Error: 'e' must be followed by a sign or digit (e.g., 1e)
      return (Token){T_ERROR, token_start, l->current_pos - start_pos};
    }

    // Consume all subsequent digits of the exponent part
    while (l->current_pos < l->source_len &&
           is_digit(l->source[l->current_pos])) {
      l->current_pos++;
    }
  }

  // Successful termination of number token
  return (Token){T_NUMBER, token_start, l->current_pos - start_pos};
  /*#endregion*/
}

static Token inner_next(T *l) {
  /*#region*/
  skip_whitespace(l);

  if (l->current_pos >= l->source_len) {
    return (Token){T_EOF, NULL, 0};
  }

  const char *token_start = l->source + l->current_pos;
  char c = *token_start;
  l->current_pos++; // Advance one character initially

  switch (c) {
  // Structural Tokens
  case '{':
    return (Token){T_BRACE_OPEN, token_start, 1};
  case '}':
    return (Token){T_BRACE_CLOSE, token_start, 1};
  case '[':
    return (Token){T_BRACKET_OPEN, token_start, 1};
  case ']':
    return (Token){T_BRACKET_CLOSE, token_start, 1};
  case ':':
    return (Token){T_COLON, token_start, 1};
  case ',':
    return (Token){T_COMMA, token_start, 1};

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
    return (Token){T_ERROR, token_start, 1}; // Unexpected character
  }
  /*#endregion*/
}

void lexer_init(T **l, const char *source, size_t len) {
  /*#region*/
  assert(*l);
  assert(source);
  assert(len);
  Lexer *self = *l;
  self->source = source;
  self->source_len = len;
  self->current_pos = 0;
  self->window = ALLOC(sizeof(Jsonv_SlidingWindow));
  // self->stack = ALLOC(jsonv_bs_sizeof());
  // jsonv_bs_init(self->stack);
  jsonv_sw_new(self->window);
  /*#endregion*/
}

Token lexer_current_token(T *l) {
  /*#region*/
  return jsonv_sw_get_by_order(l->window, 0);
  /*#endregion*/
}

Token lexer_next_token(T *l) {
  /*#region*/
  TokenType type;
  Token current = inner_next(l);
  jsonv_sw_push(l->window, current);
  type = current.type;
  if (jsonv_sw_count(l->window) == 1) {
    if (type != T_BRACE_OPEN)
      return (Token){.type = T_ERROR};
    // jsonv_bs_push(l->stack, BRACE);
    return current;
  }

  return current;
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
  free(lexer->window);
  // free(lexer->stack);
  free(lexer);
  l = NULL;
  /*#endregion*/
}
