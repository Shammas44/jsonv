#ifndef _JSONV_AST_H_INCLUDED
#define _JSONV_AST_H_INCLUDED
#include "lexer.h"
#include "stack.h"
#include <stdint.h>

typedef enum {
  AST_LEAF,
  AST_OBJECT,
  AST_ARRAY,
  AST_STRING,
  AST_NUMBER,
  AST_TRUE,
  AST_FALSE,
  AST_NULL,
  AST_SKIPPED
} ASTNodeType;

typedef struct ASTNode {
  Token token;      // To store the actual value (string, number, etc.)
  int parent;       // Link to the parent, NULL on root
  ASTNodeType type; // type of node
  int first_child;  // Index in the nodes array
  int next_sibling; // Index of the next item in the same object/array
} ASTNode;

void jsonv_ast(Lexer *it, Stack *ast, Stack *control, Stack *children);
int jsonv_find_property(ASTNode *json_pool, int object_idx, const char *key);
void print_ast(Stack *ast, int ast_idx, int indent);
void print_token(TokenType type);
char *get_error_path(Stack *children, Stack *nodes);
char *get_node_path(Stack *nodes, int node_idx);

#endif
