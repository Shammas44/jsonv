#ifndef _JSONV_HINT_H_INCLUDED
#define _JSONV_HINT_H_INCLUDED
#include "token.h"

typedef struct {
  int start;
  int end;
} jsonv_Hint;

#define HINT(json, hint, buff) hint_to_char(json, hint, buff)
#define TOK(tok, buff) tok_to_char(tok, buff)

void hint_to_char(const char *json, jsonv_Hint hint, char *buff);
void tok_to_char(Token tok, char *buff);

#endif
