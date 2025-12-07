#ifndef _JSONV_TOKEN_H_INCLUDED
#define _JSONV_TOKEN_H_INCLUDED
#include <jsmn/jsmn.h>
#define T jsonv_tokiterator

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
