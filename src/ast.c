#include "ast.h"
#include "assert.h"
#include "lexer.h"
#include "stack.h"
#include <stdio.h>
#include <string.h>

#define MARK_OBJECT_END 200
#define MARK_ARRAY_END 201

#define PUSH(stack, value) stack_push((stack), &(typeof(value)){(value)})

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

static ASTNode new_node(ASTNodeType type, Token t);

extern const Except MALFORMED_JSON;
extern const Except MAXIMUM_NESTED_DEPTH_REACHED;

static bool token_equals(Token a, Token b) {
  /*#region*/
  assert(a.type == T_STRING);
  assert(b.type == T_STRING);
  if (a.string.length != b.string.length)
    return false;
  // Both are empty? Consider them equal (or unequal depending on your needs)
  if (a.string.length == 0)
    return true;
  // Compare bytes (Token start is not guaranteed to be null-terminated)
  return memcmp(a.string.start, b.string.start, a.string.length) == 0;
  /*#endregion*/
}

void print_token(TokenType type) {
  /*#region*/
  static const char *names[] = {
      [T_BRACE_OPEN] = "T_BRACE_OPEN",
      [T_BRACE_CLOSE] = "T_BRACE_CLOSE",
      [T_BRACKET_OPEN] = "T_BRACKET_OPEN",
      [T_BRACKET_CLOSE] = "T_BRACKET_CLOSE",
      [T_STRING] = "T_STRING",
      [T_NUMBER] = "T_NUMBER",
      [T_NULL] = "T_NULL",
      [T_TRUE] = "T_TRUE",
      [T_FALSE] = "T_FALSE",
      [T_COLON] = "T_COLON",
      [T_COMMA] = "T_COMMA",
      [T_EOF] = "T_EOF",
      [T_ERROR] = "T_ERROR",

      [RULE_JSON] = "RULE_JSON",
      [RULE_VALUE] = "RULE_VALUE",
      [RULE_OBJECT] = "RULE_OBJECT",
      [RULE_MEMBERS] = "RULE_MEMBERS",
      [RULE_PAIR] = "RULE_PAIR",
      [RULE_ARRAY] = "RULE_ARRAY",
      [RULE_ELEMENTS] = "RULE_ELEMENTS",
      [RULE_NEXT_MEMBER] = "RULE_NEXT_MEMBER",
      [RULE_NEXT_ELEMENT] = "RULE_NEXT_ELEMENT",

      [MARK_OBJECT_END] = "MARK_OBJECT_END",
      [MARK_ARRAY_END] = "MARK_ARRAY_END",
  };

  if ((unsigned)type < sizeof(names) / sizeof(names[0]) && names[type])
    puts(names[type]);
  else
    puts("UNKNOWN_TOKEN");
  /*#endregion*/
}

static ASTNode new_node(ASTNodeType type, Token t) {
  ASTNode node;
  node.type = type;
  node.token = t;
  node.first_child = -1;
  node.next_sibling = -1;
  return node;
}

bool jsonv_ast(Lexer *lexer, Stack *nodes, Stack *children, Stack *controls) {
  /*#region*/
  // 1. Reset Stacks
  nodes->top = -1;
  children->top = -1;
  controls->top = -1;

  // Shared sentinel variable to avoid scope issues
  int sentinel = -1;

  PUSH(controls, RULE_JSON);
  Token c = lexer_next_token(lexer);
  Token n = lexer_next_token(lexer);

  while (controls->top >= 0) {
    int expected = *(int *)stack_pop(controls);

    // --- MATCHING TERMINALS ---
    if (expected < 100) {
      if (expected == (int)c.type) {
        if (c.type == T_STRING || c.type == T_NUMBER || c.type == T_TRUE ||
            c.type == T_FALSE || c.type == T_NULL) {

          ASTNode leaf = new_node(AST_LEAF, c);
          stack_push(nodes, &leaf);
          int idx = nodes->top;
          stack_push(children, &idx);
        }
        c = n;
        n = lexer_next_token(lexer);
        if (n.type == T_EOF && controls->top == -1) {
          break;
        }
        continue;
      }
      return false;
    }

    // --- EXPANDING RULES ---
    switch (expected) {
    case RULE_JSON:
      PUSH(controls, T_EOF);
      PUSH(controls, RULE_OBJECT);
      break;

    case RULE_VALUE:
      if (c.type == T_BRACE_OPEN) {
        PUSH(controls, RULE_OBJECT);
      } else if (c.type == T_BRACKET_OPEN) {
        PUSH(controls, RULE_ARRAY);
      } else if (c.type == T_STRING || c.type == T_NUMBER || c.type == T_TRUE ||
                 c.type == T_FALSE || c.type == T_NULL) {
        PUSH(controls, c.type);
      } else
        return false;
      break;

    case RULE_OBJECT: {
      PUSH(controls, MARK_OBJECT_END);
      PUSH(children, sentinel); // Safe push
      PUSH(controls, T_BRACE_CLOSE);
      if (n.type != T_BRACE_CLOSE)
        PUSH(controls, RULE_MEMBERS);
      PUSH(controls, T_BRACE_OPEN);
      break;
    }

    case RULE_ARRAY: {
      PUSH(controls, MARK_ARRAY_END);
      PUSH(children, sentinel); // Safe push
      PUSH(controls, T_BRACKET_CLOSE);
      // Check lookahead 'n' to decide if we push elements
      if (n.type != T_BRACKET_CLOSE)
        PUSH(controls, RULE_ELEMENTS);
      PUSH(controls, T_BRACKET_OPEN);
      break;
    }

    case RULE_MEMBERS:
      PUSH(controls, RULE_NEXT_MEMBER);
      PUSH(controls, RULE_PAIR);
      break;

    case RULE_PAIR:
      PUSH(controls, RULE_VALUE);
      PUSH(controls, T_COLON);
      PUSH(controls, T_STRING);
      break;

    case RULE_NEXT_MEMBER:
      if (c.type == T_COMMA) {
        PUSH(controls, RULE_MEMBERS);
        PUSH(controls, T_COMMA);
      }
      break;

    case RULE_ELEMENTS:
      PUSH(controls, RULE_NEXT_ELEMENT);
      PUSH(controls, RULE_VALUE);
      break;

    case RULE_NEXT_ELEMENT:
      if (c.type == T_COMMA) {
        PUSH(controls, RULE_ELEMENTS);
        PUSH(controls, T_COMMA);
      }
      break;

    // --- REDUCTION LOGIC ---
    case MARK_OBJECT_END: {
      ASTNode obj_node = new_node(AST_OBJECT, (Token){0});
      stack_push(nodes, &obj_node);
      int obj_idx = nodes->top;
      int head = -1;

      while (children->top >= 0 &&
             *(int *)stack_peek(children, children->top) != -1) {
        int val_idx = *(int *)stack_pop(children);
        int key_idx = *(int *)stack_pop(children);

        ASTNode *pool = (ASTNode *)nodes->data;

        // --- DUPLICATE CHECK START ---
        // Traverse the list of keys we have ALREADY linked
        int scanner = head;
        while (scanner != -1) {
          Token existing_key = pool[scanner].token;
          Token new_key = pool[key_idx].token;

          if (token_equals(existing_key, new_key)) {
            // printf("Error: Duplicate key found: '%.*s'\n",
            // (int)new_key.length,
            //        new_key.start);
            return false;
          }

          // Move to next pair: Key -> Value -> NextKey
          // 1. Get Value node
          int val_sibling = pool[scanner].next_sibling;
          // 2. Get Next Key (which is the sibling of the Value)
          scanner = pool[val_sibling].next_sibling;
        }
        // --- DUPLICATE CHECK END ---

        pool[key_idx].next_sibling = val_idx;
        pool[val_idx].next_sibling = head;
        head = key_idx;
      }
      stack_pop(children); // Pop sentinel
      ((ASTNode *)nodes->data)[obj_idx].first_child = head;
      PUSH(children, obj_idx);
      break;
    }

    case MARK_ARRAY_END: {
      ASTNode arr_node = new_node(AST_ARRAY, (Token){0});
      stack_push(nodes, &arr_node);
      int arr_idx = nodes->top;
      int head = -1;

      while (children->top >= 0 &&
             *(int *)stack_peek(children, children->top) != -1) {
        int item_idx = *(int *)stack_pop(children);

        ASTNode *pool = (ASTNode *)nodes->data;
        pool[item_idx].next_sibling = head;
        head = item_idx;
      }
      stack_pop(children); // Pop sentinel
      ((ASTNode *)nodes->data)[arr_idx].first_child = head;
      PUSH(children, arr_idx);
      break;
    }
    }
  }
  return n.type == T_EOF;
  /*#endregion*/
}

void print_ast(Stack *nodes, int index, int indent) {
  /*#region*/
  if (index == -1)
    return;

  // 1. Get pointer to the node (re-evaluate every time for safety)
  ASTNode *pool = (ASTNode *)nodes->data;
  ASTNode *node = &pool[index];

  // 2. Print Indentation
  for (int i = 0; i < indent; i++)
    printf("  ");

  // 3. Print Node Info
  switch ((int)node->type) {
  case AST_OBJECT:
    printf("OBJECT {\n");
    print_ast(nodes, node->first_child, indent + 1);
    for (int i = 0; i < indent; i++)
      printf("  ");
    printf("}\n");
    break;

  case AST_ARRAY:
    printf("ARRAY [\n");
    print_ast(nodes, node->first_child, indent + 1);
    for (int i = 0; i < indent; i++)
      printf("  ");
    printf("]\n");
    break;

  case AST_LEAF: {
    switch (node->token.type) {
    case T_NUMBER:
      printf("LEAF: %f\n", node->token.number);
      break;
    case T_NULL:
      printf("LEAF: null\n");
      break;
    case T_TRUE:
      printf("LEAF: true\n");
      break;
    case T_FALSE:
      printf("LEAF: false\n");
      break;
    default:
      printf("LEAF: %.*s\n", (int)node->token.string.length,
             node->token.string.start);
    }
    break;
  }
  }

  // 4. Print Next Sibling
  if (node->next_sibling != -1) {
    print_ast(nodes, node->next_sibling, indent);
  }
  /*#endregion*/
}
