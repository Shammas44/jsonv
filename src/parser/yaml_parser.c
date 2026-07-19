#include "yaml_parser.h"
#ifdef JSONV_YAML_SUPPORT
#include "except.h"
#include "assert.h"
#include "arena.internal.h"
#include "mem.h"
#include <string.h>
#include <stdio.h>

#define PUSH(stack, value) stack_push((stack), &(typeof(value)){(value)})
#define PEEK_INT(stack, offset)                                                \
  (*(int *)stack_peek((stack), (stack)->top - (offset)))

extern const Except MALFORMED_JSON;
extern const Except MAXIMUM_NESTED_DEPTH_REACHED;

static ASTNode new_node(ASTNodeType type, Token t) {
  /*#region*/
  ASTNode node;
  node.type = type;
  node.token = t;
  node.first_child = -1;
  node.next_sibling = -1;
  node.parent = -1;
  node.key_tree_root = -1;
  return node;
  /*#endregion*/
}

static void link_node_to_scope(Stack *nodes, Stack *scopes, int new_node_idx,
                               set_t *set, KeyTreePool *key_pool) {
  /*#region*/
  if (scopes->top < 2)
    return; // Root level, no parent

  int count = PEEK_INT(scopes, 0);
  int tail = PEEK_INT(scopes, 1);
  int parent = PEEK_INT(scopes, 2);

  ASTNode *pool = (ASTNode *)nodes->data;
  pool[new_node_idx].parent = parent;

  if (tail == -1) {
    pool[parent].first_child = new_node_idx;
  } else {
    pool[tail].next_sibling = new_node_idx;
  }

  // Value Skipping Logic
  if (pool[parent].token.type == T_BRACE_OPEN && (count % 2) != 0) {
    if (tail != -1 && pool[tail].type == AST_SKIPPED) {
      pool[new_node_idx].type = AST_SKIPPED;
    }
  }

  // Update Scope Tail and Count
  *(int *)stack_peek(scopes, scopes->top - 1) = new_node_idx;
  *(int *)stack_peek(scopes, scopes->top) = count + 1;

  // Duplicate Detection and Key Tree Insertion
  if (pool[parent].token.type == T_BRACE_OPEN) {
    if ((count % 2) == 0) {
#define SCOPED_KEY_SAFE_SIZE 512
      char scoped_key[SCOPED_KEY_SAFE_SIZE];

      Token *t = &pool[new_node_idx].token;
      int prefix_len =
          snprintf(scoped_key, SCOPED_KEY_SAFE_SIZE, "%d:", parent);

      if (prefix_len > 0) {
        size_t key_len = t->value.string.length;
        size_t total_needed = (size_t)prefix_len + key_len;

        if (total_needed < SCOPED_KEY_SAFE_SIZE && total_needed < MAX_KEY_LEN &&
            total_needed <= 65535) {

          memcpy(scoped_key + prefix_len, t->value.string.start, key_len);
          scoped_key[total_needed] = '\0';

          int result = set_insert(set, scoped_key, (uint16_t)total_needed);

          if (result == SET_KEY_ALREADY_EXIST) {
            pool[new_node_idx].type = AST_SKIPPED;
          } else {
            pool[parent].key_tree_root = key_tree_insert(
                key_pool, pool[parent].key_tree_root, new_node_idx, pool);
          }
        }
      }
    }
  }
  /*#endregion*/
}

static void parse_yaml_value(YamlLexer *lexer, Stack *nodes, Stack *scopes,
                             set_t *set, KeyTreePool *key_pool, int *depth, Token *c, Token *n);

static void parse_yaml_block_mapping(YamlLexer *lexer, Stack *nodes, Stack *scopes,
                                     set_t *set, KeyTreePool *key_pool, int *depth, Token *c, Token *n, bool is_inline) {
  /*#region*/
  // Create AST_OBJECT Node
  Token obj_token = *c;
  obj_token.type = T_BRACE_OPEN; // Force T_BRACE_OPEN for duplicate checking & shape compatibility
  ASTNode obj = new_node(AST_OBJECT, obj_token);
  stack_push(nodes, &obj);
  int obj_idx = nodes->top;
  link_node_to_scope(nodes, scopes, obj_idx, set, key_pool);

  // Push new scope
  PUSH(scopes, obj_idx);   // Parent
  PUSH(scopes, -1);        // Tail
  PUSH(scopes, 0);         // Count

  int indent_count = 0;

  while (c->type != T_EOF) {
    if (c->type == T_YAML_INDENT) {
      if (is_inline && indent_count == 0) {
        *c = *n;
        *n = yaml_lexer_next_token(lexer);
        indent_count = 1;
        continue;
      } else {
        break;
      }
    }

    if (c->type == T_YAML_DEDENT) {
      if (is_inline && indent_count == 1) {
        *c = *n;
        *n = yaml_lexer_next_token(lexer);
        indent_count = 0;
      }
      break;
    }

    if (c->type == T_YAML_BULLET) {
      break;
    }

    if (c->type != T_STRING && c->type != T_NUMBER && c->type != T_TRUE && c->type != T_FALSE && c->type != T_NULL) {
      RAISE(MALFORMED_JSON);
    }

    // Key is current token c
    ASTNode key_leaf = new_node(AST_LEAF, *c);
    stack_push(nodes, &key_leaf);
    link_node_to_scope(nodes, scopes, nodes->top, set, key_pool);

    // Consume key
    *c = *n;
    *n = yaml_lexer_next_token(lexer);

    if (c->type != T_COLON) {
      RAISE(MALFORMED_JSON);
    }

    // Consume colon
    *c = *n;
    *n = yaml_lexer_next_token(lexer);

    // Parse value recursively
    parse_yaml_value(lexer, nodes, scopes, set, key_pool, depth, c, n);
  }

  // Pop scope
  stack_pop(scopes); // Count
  stack_pop(scopes); // Tail
  stack_pop(scopes); // Parent
  /*#endregion*/
}

static void parse_yaml_block_sequence(YamlLexer *lexer, Stack *nodes, Stack *scopes,
                                      set_t *set, KeyTreePool *key_pool, int *depth, Token *c, Token *n, bool is_inline) {
  /*#region*/
  // Create AST_ARRAY Node
  Token arr_token = *c;
  arr_token.type = T_BRACKET_OPEN;
  ASTNode arr = new_node(AST_ARRAY, arr_token);
  stack_push(nodes, &arr);
  int arr_idx = nodes->top;
  link_node_to_scope(nodes, scopes, arr_idx, set, key_pool);

  // Push new scope
  PUSH(scopes, arr_idx);   // Parent
  PUSH(scopes, -1);        // Tail
  PUSH(scopes, 0);         // Count

  int indent_count = 0;

  while (c->type != T_EOF) {
    if (c->type == T_YAML_INDENT) {
      if (is_inline && indent_count == 0) {
        *c = *n;
        *n = yaml_lexer_next_token(lexer);
        indent_count = 1;
        continue;
      } else {
        break;
      }
    }

    if (c->type == T_YAML_DEDENT) {
      if (is_inline && indent_count == 1) {
        *c = *n;
        *n = yaml_lexer_next_token(lexer);
        indent_count = 0;
      }
      break;
    }

    if (c->type != T_YAML_BULLET) {
      break;
    }

    // Consume bullet
    *c = *n;
    *n = yaml_lexer_next_token(lexer);

    // Parse item value recursively
    parse_yaml_value(lexer, nodes, scopes, set, key_pool, depth, c, n);
  }

  // Pop scope
  stack_pop(scopes); // Count
  stack_pop(scopes); // Tail
  stack_pop(scopes); // Parent
  /*#endregion*/
}
static void parse_yaml_value(YamlLexer *lexer, Stack *nodes, Stack *scopes,
                             set_t *set, KeyTreePool *key_pool, int *depth, Token *c, Token *n) {
  /*#region*/
  (*depth)++;
  if (*depth > MAX_YAML_DEPTH) {
    RAISE(MAXIMUM_NESTED_DEPTH_REACHED);
  }

  // Check for empty/omitted value (null)
  if (c->type == T_YAML_DEDENT || c->type == T_EOF ||
      (*depth > 1 && c->on_new_line && c->type != T_YAML_INDENT && c->type != T_BRACE_OPEN && c->type != T_BRACKET_OPEN)) {
    Token null_token = {
      .type = T_NULL,
      .value = {0},
      .on_new_line = false
    };
    ASTNode leaf = new_node(AST_LEAF, null_token);
    stack_push(nodes, &leaf);
    link_node_to_scope(nodes, scopes, nodes->top, set, key_pool);
    (*depth)--;
    return;
  }

  if (c->type == T_BRACE_OPEN) {
    // Flow mapping
    Token obj_token = *c;
    ASTNode obj = new_node(AST_OBJECT, obj_token);
    stack_push(nodes, &obj);
    int obj_idx = nodes->top;
    link_node_to_scope(nodes, scopes, obj_idx, set, key_pool);

    PUSH(scopes, obj_idx);   // Parent
    PUSH(scopes, -1);        // Tail
    PUSH(scopes, 0);         // Count

    // Consume '{'
    *c = *n;
    *n = yaml_lexer_next_token(lexer);

    while (c->type != T_BRACE_CLOSE && c->type != T_EOF) {
      if (c->type != T_STRING) {
        RAISE(MALFORMED_JSON);
      }
      ASTNode key_leaf = new_node(AST_LEAF, *c);
      stack_push(nodes, &key_leaf);
      link_node_to_scope(nodes, scopes, nodes->top, set, key_pool);

      *c = *n;
      *n = yaml_lexer_next_token(lexer);

      if (c->type != T_COLON) {
        RAISE(MALFORMED_JSON);
      }
      *c = *n;
      *n = yaml_lexer_next_token(lexer);

      parse_yaml_value(lexer, nodes, scopes, set, key_pool, depth, c, n);

      if (c->type == T_COMMA) {
        *c = *n;
        *n = yaml_lexer_next_token(lexer);
      } else if (c->type != T_BRACE_CLOSE) {
        RAISE(MALFORMED_JSON);
      }
    }
    if (c->type != T_BRACE_CLOSE) {
      RAISE(MALFORMED_JSON);
    }
    // Consume '}'
    *c = *n;
    *n = yaml_lexer_next_token(lexer);

    stack_pop(scopes); // Count
    stack_pop(scopes); // Tail
    stack_pop(scopes); // Parent
  }
  else if (c->type == T_BRACKET_OPEN) {
    // Flow sequence
    Token arr_token = *c;
    ASTNode arr = new_node(AST_ARRAY, arr_token);
    stack_push(nodes, &arr);
    int arr_idx = nodes->top;
    link_node_to_scope(nodes, scopes, arr_idx, set, key_pool);

    PUSH(scopes, arr_idx);   // Parent
    PUSH(scopes, -1);        // Tail
    PUSH(scopes, 0);         // Count

    // Consume '['
    *c = *n;
    *n = yaml_lexer_next_token(lexer);

    while (c->type != T_BRACKET_CLOSE && c->type != T_EOF) {
      parse_yaml_value(lexer, nodes, scopes, set, key_pool, depth, c, n);

      if (c->type == T_COMMA) {
        *c = *n;
        *n = yaml_lexer_next_token(lexer);
      } else if (c->type != T_BRACKET_CLOSE) {
        RAISE(MALFORMED_JSON);
      }
    }
    if (c->type != T_BRACKET_CLOSE) {
      RAISE(MALFORMED_JSON);
    }
    // Consume ']'
    *c = *n;
    *n = yaml_lexer_next_token(lexer);

    stack_pop(scopes); // Count
    stack_pop(scopes); // Tail
    stack_pop(scopes); // Parent
  }
  else if (c->type == T_YAML_INDENT) {
    // Consume INDENT
    *c = *n;
    *n = yaml_lexer_next_token(lexer);

    if (c->type == T_YAML_BULLET) {
      parse_yaml_block_sequence(lexer, nodes, scopes, set, key_pool, depth, c, n, false);
    } else {
      parse_yaml_block_mapping(lexer, nodes, scopes, set, key_pool, depth, c, n, false);
    }

    if (c->type != T_YAML_DEDENT && c->type != T_EOF) {
      RAISE(MALFORMED_JSON);
    }
    if (c->type == T_YAML_DEDENT) {
      // Consume DEDENT
      *c = *n;
      *n = yaml_lexer_next_token(lexer);
    }
  }
  else if (c->type == T_YAML_BULLET) {
    parse_yaml_block_sequence(lexer, nodes, scopes, set, key_pool, depth, c, n, true);
  }
  else if (c->type == T_STRING || c->type == T_NUMBER || c->type == T_TRUE || c->type == T_FALSE || c->type == T_NULL) {
    if (n->type == T_COLON) {
      parse_yaml_block_mapping(lexer, nodes, scopes, set, key_pool, depth, c, n, true);
    } else {
      ASTNode leaf = new_node(AST_LEAF, *c);
      stack_push(nodes, &leaf);
      link_node_to_scope(nodes, scopes, nodes->top, set, key_pool);

      *c = *n;
      *n = yaml_lexer_next_token(lexer);
    }
  }
  else {
    RAISE(MALFORMED_JSON);
  }

  (*depth)--;
  /*#endregion*/
}

void yaml_parse_ast(YamlLexer *lexer, Stack *nodes, Stack *scopes, Stack *controls,
                    set_t *set, KeyTreePool *key_pool) {
  /*#region*/
  (void)controls; // Kept in signature for consistency with JSON parser
  nodes->top = -1;
  scopes->top = -1;
  int depth = 0;

  Token c = yaml_lexer_next_token(lexer);
  Token n = yaml_lexer_next_token(lexer);

  parse_yaml_value(lexer, nodes, scopes, set, key_pool, &depth, &c, &n);

  if (c.type != T_EOF) {
    RAISE(MALFORMED_JSON);
  }
  /*#endregion*/
}

void yaml_parse_to_ast(
    Jsonv_Arena *arena,
    YamlLexer *lexer,
    size_t yaml_length,
    size_t est_value_count,
    Stack *out_ast,
    KeyTreePool *out_keytree,
    set_t *out_set
) {
  /*#region*/
  assert(arena);
  assert(lexer);
  assert(out_ast);
  assert(out_keytree);
  assert(out_set);

  // 1. AST Stack
  size_t ast_storage_size = yaml_length * sizeof(ASTNode);
  void *ast_storage = arena_alloc(arena, ast_storage_size);
  stack_init(out_ast, sizeof(ASTNode), ast_storage, ast_storage_size);

  // 2. Transient Scopes Stack
  Stack scopes = {0};
  size_t scopes_storage_size = yaml_length * sizeof(int);
  void *scopes_storage = arena_alloc(arena, scopes_storage_size);
  stack_init(&scopes, sizeof(int), scopes_storage, scopes_storage_size);

  // 3. Element Set
  size_t capacity = (est_value_count * 12) / 10;
  size_t keys_capacity = set_next_power_of_two(capacity);
  size_t set_storage_size = sizeof(entry_t) * keys_capacity;
  entry_t *set_data = (entry_t *)arena_alloc(arena, set_storage_size);
  set_init(out_set, set_data, keys_capacity);

  // 4. Key Tree
  size_t key_storage_size = sizeof(KeyNode) * keys_capacity;
  KeyNode *keytree_data = (KeyNode *)arena_alloc(arena, key_storage_size);
  key_tree_init(out_keytree, keytree_data, keys_capacity);

  // 5. Invoke parser
  yaml_parse_ast(lexer, out_ast, &scopes, NULL, out_set, out_keytree);
  /*#endregion*/
}
#endif // JSONV_YAML_SUPPORT
