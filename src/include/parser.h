#ifndef _JSONV_PARSER_H_INCLUDED
#define _JSONV_PARSER_H_INCLUDED
#include "ast.h"
#include "value.h"
#include "shape.h"
#include "keytree.h"
#include "lexer.h"
#include "set.h"
#include "stack.h"

void jsonv_ast(Lexer *it, Stack *ast, Stack *scopes, Stack *controls,
               set_t *set, KeyTreePool *keytree);
int jsonv_find_property(ASTNode *json_pool, int object_idx, const char *key);
void print_ast(Stack *ast, int ast_idx, int indent);
char *get_error_path(Stack *children, Stack *nodes);
char *get_node_path(Stack *nodes, int node_idx, size_t size);
void print_ast_alphabetical(ASTNode *pool, KeyTreePool *key_pool, int node_idx,
                            int indent);

Value ast_to_value(ASTNode *pool, KeyTreePool *key_pool, int node_idx, Shape *shape_root);

#endif
