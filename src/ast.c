#include "ast.h"
#include "assert.h"
#include "error.h"
#include "lexer.h"
#include "node.h"
#include "token.h"
#include <stdio.h>

extern const Except MALFORMED_JSON;
// extern const Except MAXIMUM_NESTED_DEPTH_REACHED;

void jsonv_ast(const char *json, Lexer *it, Jsonv_Node *parent,
               Jsonv_path *path, Jsonv_error_stack *errors, size_t index) {
  /*#region*/
  (void)(json);
  (void)(parent);
  (void)(path);
  (void)(errors);
  (void)(index);
  validate_json(it);
  /*#endregion*/
}

// Grammar Rules (Non-Terminals)
// We start these at a high number to avoid clashing with your TokenType values
typedef enum {
  RULE_JSON = 100,
  RULE_VALUE,
  RULE_OBJECT,
  RULE_MEMBERS,
  RULE_PAIR,
  RULE_ARRAY,
  RULE_ELEMENTS,
  RULE_NEXT_MEMBER,
  RULE_NEXT_ELEMENT
} RuleType;

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

// typedef struct ASTNode {
//   ASTNodeType type;
//   union {
//     double number;
//     List children;
//     struct {
//       const char *val;
//       size_t length;
//     } string;
//   };
// } ASTNode;

void print_token(TokenType type) {
  /*#region*/
  switch ((int)type) {
  case T_BRACE_OPEN:
    puts("T_BRACE_OPEN");
    break;
  case T_BRACE_CLOSE:
    puts("T_BRACE_CLOSE");
    break;
  case T_BRACKET_OPEN:
    puts("T_BRACKET_OPEN");
    break;
  case T_BRACKET_CLOSE:
    puts("T_BRACKET_CLOSE");
    break;
  case T_STRING:
    puts("T_STRING");
    break;
  case T_NUMBER:
    puts("T_NUMBER");
    break;
  case T_NULL:
    puts("T_NULL");
    break;
  case T_TRUE:
    puts("T_TRUE");
    break;
  case T_FALSE:
    puts("T_FALSE");
    break;
  case T_COLON:
    puts("T_COLON");
    break;
  case T_COMMA:
    puts("T_COMMA");
    break;
  case T_EOF:
    puts("T_EOF");
    break;
  case T_ERROR:
    puts("T_ERROR");
    break;
  case RULE_JSON:
    puts("RULE_JSON");
    break;
  case RULE_VALUE:
    puts("RULE_VALUE");
    break;
  case RULE_OBJECT:
    puts("RULE_OBJECT");
    break;
  case RULE_MEMBERS:
    puts("RULE_MEMBERS");
    break;
  case RULE_PAIR:
    puts("RULE_PAIR");
    break;
  case RULE_ARRAY:
    puts("RULE_ARRAY");
    break;
  case RULE_ELEMENTS:
    puts("RULE_ELEMENTS");
    break;
  case RULE_NEXT_MEMBER:
    puts("RULE_NEXT_MEMBER");
    break;
  case RULE_NEXT_ELEMENT:
    puts("RULE_NEXT_ELEMENT");
    break;
  }
  /*#endregion*/
}

#define MARK_OBJECT_END 200
#define MARK_ARRAY_END 201
#define MAX_STACK 256
#define MAX_NODES 1024

typedef struct {
  ASTNodeType type;
  int first_child;  // Index in the nodes array
  int next_sibling; // Index of the next item in the same object/array
  Token token;      // To store the actual value (string, number, etc.)
} ASTNode;

ASTNode ast_pool[MAX_NODES];
int node_count = 0;

// Data Stack: Stores indices of completed nodes waiting to be attached to a parent
int data_stack[MAX_STACK];
int data_top = -1;

// Control Stack for Grammar
int control_stack[MAX_STACK];
int control_top = -1;

void push(int s) {
  /*#region*/
  if (control_top < MAX_STACK - 1)
    control_stack[++control_top] = s;
  /*#endregion*/
}

int pop() {
  /*#region*/
  return (control_top >= 0) ? control_stack[control_top--] : -1;
  /*#endregion*/
}

int new_node(ASTNodeType type, Token t) {
  if (node_count >= MAX_NODES)
    return -1;
  ast_pool[node_count].type = type;
  ast_pool[node_count].token = t;
  ast_pool[node_count].first_child = -1;
  ast_pool[node_count].next_sibling = -1;
  return node_count++;
}

// 2. The Parser Function
bool validate_json(Lexer *lexer) {
  control_top = -1;
  data_top = -1;
  node_count = 0;

  push(RULE_JSON);
  Token c = lexer_next_token(lexer);
  Token n = lexer_next_token(lexer);

  while (control_top >= 0) {
    int expected = pop();

    // MATCHING TERMINALS
    if (expected < 100) {
      if (expected == (int)c.type) {
        // If it's a value (String, Number, Bool, Null), push to data stack
        if (c.type == T_STRING || c.type == T_NUMBER || c.type == T_TRUE ||
            c.type == T_FALSE || c.type == T_NULL) {
          data_stack[++data_top] = new_node(AST_LEAF, c);
        }
        c = n;
        n = lexer_next_token(lexer);
        if (n.type == T_EOF && control_top == -1)
          break;
        continue;
      }
      return false;
    }

    // EXPANDING RULES
    switch (expected) {
    case RULE_JSON:
      push(RULE_OBJECT);
      break;

    case RULE_VALUE:
      if (c.type == T_BRACE_OPEN)
        push(RULE_OBJECT);
      else if (c.type == T_BRACKET_OPEN)
        push(RULE_ARRAY);
      else if (c.type == T_STRING || c.type == T_NUMBER || c.type == T_TRUE ||
               c.type == T_FALSE || c.type == T_NULL)
        push(c.type);
      else
        return false;
      break;

    case RULE_OBJECT:
      push(MARK_OBJECT_END);
      data_stack[++data_top] = -1; // Sentinel for Object start
      push(T_BRACE_CLOSE);
      if (n.type != T_BRACE_CLOSE)
        push(RULE_MEMBERS);
      push(T_BRACE_OPEN);
      break;

    case RULE_ARRAY:
      push(MARK_ARRAY_END);
      data_stack[++data_top] = -1; // Sentinel for Array start
      push(T_BRACKET_CLOSE);
      if (n.type != T_BRACKET_CLOSE)
        push(RULE_ELEMENTS);
      push(T_BRACKET_OPEN);
      break;

    case RULE_MEMBERS:
      push(RULE_NEXT_MEMBER);
      push(RULE_PAIR);
      break;

    case RULE_PAIR:
      push(RULE_VALUE);
      push(T_COLON);
      push(T_STRING);
      break;

    case RULE_NEXT_MEMBER:
      if (c.type == T_COMMA) {
        push(RULE_MEMBERS);
        push(T_COMMA);
      }
      break;

    case RULE_ELEMENTS:
      push(RULE_NEXT_ELEMENT);
      push(RULE_VALUE);
      break;

    case RULE_NEXT_ELEMENT:
      if (c.type == T_COMMA) {
        push(RULE_ELEMENTS);
        push(T_COMMA);
      }
      break;

    // REDUCTION LOGIC
    case MARK_OBJECT_END: {
      int obj_idx = new_node(AST_OBJECT, (Token){0});
      int last_pair = -1;
      while (data_top >= 0 && data_stack[data_top] != -1) {
        int val_idx = data_stack[data_top--];
        int key_idx = data_stack[data_top--];
        ast_pool[key_idx].next_sibling = val_idx;
        ast_pool[val_idx].next_sibling = last_pair;
        last_pair = key_idx;
      }
      data_top--; // Pop sentinel
      ast_pool[obj_idx].first_child = last_pair;
      data_stack[++data_top] = obj_idx;
      break;
    }

    case MARK_ARRAY_END: {
      int arr_idx = new_node(AST_ARRAY, (Token){0});
      int last_item = -1;
      while (data_top >= 0 && data_stack[data_top] != -1) {
        int item_idx = data_stack[data_top--];
        ast_pool[item_idx].next_sibling = last_item;
        last_item = item_idx;
      }
      data_top--; // Pop sentinel
      ast_pool[arr_idx].first_child = last_item;
      data_stack[++data_top] = arr_idx;
      break;
    }
    }
  }
  int root_node = data_stack[0];
  print_ast(root_node, 0);
  return n.type == T_EOF;
}

void print_ast(int node_idx, int indent) {
  if (node_idx == -1)
    return;

  ASTNode *node = &ast_pool[node_idx];

  // Print indentation
  for (int i = 0; i < indent; i++)
    printf("  ");

  switch ((int)node->type) {
  case AST_LEAF:
    // Print the actual text from the lexer token
    printf("LEAF: %.*s\n", (char)node->token.length, node->token.start);
    // Move to next sibling (e.g., if this was a Key, the sibling is the Value)
    print_ast(node->next_sibling, indent);
    break;

  case AST_OBJECT:
    printf("OBJECT {\n");
    print_ast(node->first_child, indent + 1);
    for (int i = 0; i < indent; i++)
      printf("  ");
    printf("}\n");
    // Move to next sibling after closing this object
    print_ast(node->next_sibling, indent);
    break;

  case AST_ARRAY:
    printf("ARRAY [\n");
    print_ast(node->first_child, indent + 1);
    for (int i = 0; i < indent; i++)
      printf("  ");
    printf("]\n");
    // Move to next sibling after closing this array
    print_ast(node->next_sibling, indent);
    break;
  }
}
