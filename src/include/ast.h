#ifndef _JSONV_AST_H_INCLUDED
#define _JSONV_AST_H_INCLUDED
#include "error.h"
#include "node.h"

void jsonv_ast(const char *json, Lexer *it, Jsonv_Node *parent,
               Jsonv_path *path, Jsonv_error_stack *errors, size_t index);

bool validate_json(Lexer*lexer);
void print_token(TokenType type);
void print_ast(int node_idx, int depth);
#endif
