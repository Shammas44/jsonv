#ifndef _JSONV_HINT_H_INCLUDED
#define _JSONV_HINT_H_INCLUDED
#include "jsmn.h"

typedef struct {
  int start;
  int end;
} jsonv_Hint;

#define HINT(json, hint, buff) hint_to_char(json, hint, buff)
#define TOK(json, tok, buff) jsmntok_to_char(json, tok, buff)

void hint_to_char(const char *json, jsonv_Hint hint, char *buff);
void jsmntok_to_char(const char *json, jsmntok_t tok, char *buff);

#endif
