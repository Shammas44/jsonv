#ifndef _JSONV_TOKEN_H_INCLUDED
#define _JSONV_TOKEN_H_INCLUDED
#include "jsmn.h"
#include <stdio.h>

#define T jsonv_tokiterator

typedef enum {
  T_BRACE_OPEN,
  T_BRACE_CLOSE,
  T_BRACKET_OPEN,
  T_BRACKET_CLOSE,
  T_STRING,
  T_NUMBER,
  T_NULL,
  T_TRUE,
  T_FALSE,
  T_COLON,
  T_COMMA,
  T_EOF,
  T_ERROR
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

typedef struct T T;

T *jsonv_tokiterator_new(const char *json, jsmntok_t **tokens, int tok_count);
jsmntok_t *jsonv_tokiterator_next(T *self);
jsmntok_t *jsonv_tokiterator_current(T *self);
void jsonv_tokiterator_free(T **self);
int jsonv_tokiterator_index(T *self);
char *jsonv_extract_token_string(const char *json, const jsmntok_t *token);
jsmntok_t *jsonv_tokiterator_relative(T *self, int index);
int jsonv_extract_token_int(const char *json, const jsmntok_t *token);

#undef T
#endif
