#ifndef _JSONV_PARSER_H_INCLUDED
#define _JSONV_PARSER_H_INCLUDED
#include "ast.h"
#include "value.h"
#include "shape.h"
#include "keytree.h"
#include "lexer.h"
#include "set.h"
#include "stack.h"

#include "arena.h"

void parse_ast(Lexer *it, Stack *ast, Stack *scopes, Stack *controls,
               set_t *set, KeyTreePool *keytree);
int find_property(ASTNode *json_pool, int object_idx, const char *key);
void print_ast(Stack *ast, int ast_idx, int indent);
char *get_error_path(Stack *children, Stack *nodes);
char *get_node_path(Stack *nodes, int node_idx, size_t size);
void print_ast_alphabetical(ASTNode *pool, KeyTreePool *key_pool, int node_idx,
                            int indent);

void parse_to_ast(
    Jsonv_Arena *arena,
    Lexer *lexer,
    size_t json_length,
    size_t est_value_count,
    Stack *out_ast,
    KeyTreePool *out_keytree,
    set_t *out_set
);

Value ast_to_value(ASTNode *pool, KeyTreePool *key_pool, int node_idx, Shape *shape_root, Jsonv_Arena *arena);

#endif
