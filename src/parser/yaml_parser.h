#ifndef _JSONV_YAML_PARSER_H_INCLUDED
#define _JSONV_YAML_PARSER_H_INCLUDED

#include "ast.h"
#include "shape.internal.h"
#include "keytree.h"
#include "yaml_lexer.h"
#include "set.h"
#include "stack.h"
#include "arena.h"

void yaml_parse_ast(YamlLexer *lexer, Stack *nodes, Stack *scopes, Stack *controls,
                    set_t *set, KeyTreePool *key_pool);

void yaml_parse_to_ast(
    Jsonv_Arena *arena,
    YamlLexer *lexer,
    size_t yaml_length,
    size_t est_value_count,
    Stack *out_ast,
    KeyTreePool *out_keytree,
    set_t *out_set
);

#endif
