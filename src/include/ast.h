#ifndef _JSONV_AST_H_INCLUDED
#define _JSONV_AST_H_INCLUDED
#include "lexer.h"
#include "stack.h"

typedef enum {
  AST_LEAF,
  AST_OBJECT,
  AST_ARRAY,
  AST_STRING,
  AST_NUMBER,
  AST_TRUE,
  AST_FALSE,
  AST_NULL,
} ASTNodeType;

typedef struct {
  ASTNodeType type;
  int first_child;  // Index in the nodes array
  int next_sibling; // Index of the next item in the same object/array
  Token token;      // To store the actual value (string, number, etc.)
} ASTNode;

bool jsonv_ast(Lexer *it, Stack *ast, Stack *control, Stack *children);
void print_ast(Stack *ast, int ast_idx, int indent);
void print_token(TokenType type);

#endif
