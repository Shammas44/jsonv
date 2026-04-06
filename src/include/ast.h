#ifndef _JSONV_AST_H_INCLUDED
#define _JSONV_AST_H_INCLUDED
#include "token.h"
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
  int first_child;  // Index in the nodes array (stack)
  int next_sibling; // Index of the next item in the same object/array
  int key_tree_root;
} ASTNode;

#endif
