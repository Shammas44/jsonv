#ifndef _JSONV_LEXER_H_INCLUDED
#define _JSONV_LEXER_H_INCLUDED
#include "token.h"
#define T Lexer

typedef struct T {
  const unsigned char *source; // The entire JSON string input
  size_t source_len;
  size_t current_pos;
} T;

void lexer_init(T *l, const unsigned char *source, size_t len);
Token lexer_next_token(T *l);
size_t lexer_sizeof(void);
void lexer_free(T **l);
#undef T
#endif
