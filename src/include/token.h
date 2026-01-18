#ifndef _JSONV_TOKEN_H_INCLUDED
#define _JSONV_TOKEN_H_INCLUDED
#include <stdio.h>

typedef enum {
  // CONTROL
  T_ERROR,
  T_EOF,
  // SYNTAXE
  T_COLON,
  T_COMMA,
  // NEST
  T_BRACE_OPEN,
  T_BRACE_CLOSE,
  T_BRACKET_OPEN,
  T_BRACKET_CLOSE,
  // LITERAL
  T_STRING,
  T_NUMBER,
  T_NULL,
  T_TRUE,
  T_FALSE,
} TokenType;

typedef struct {
  TokenType type;
  union {
    double number;
    struct {
      // Pointers to the start and end of the token in the source string.
      // This avoids copying the token value until needed.
      const unsigned char *start;
      size_t length;
    } string;
  };
} Token;

#endif
