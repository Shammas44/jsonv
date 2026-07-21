#ifndef _JSONV_TOKEN_H
#define _JSONV_TOKEN_H
#include <stdio.h>
#include <stdbool.h>

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
#ifdef JSONV_YAML_SUPPORT
  // YAML
  T_YAML_INDENT,
  T_YAML_DEDENT,
  T_YAML_BULLET,
#endif
} TokenType;

  typedef union {
    struct {
      const unsigned char *start;
      size_t length;
    } string;
    struct {
      const unsigned char *start;
      size_t length;
    } raw_number;
  } TokenValue;

typedef struct {
  TokenValue value;
  TokenType type;
  bool on_new_line;
  bool has_escape;
} Token;

#endif
