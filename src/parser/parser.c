#include "parser.h"
#include "shape.h"
#include "obj.h"
#include "arr.h"
#include "assert.h"
#include "keytree.h"
#include "lexer.h"
#include "set.h"
#include "stack.h"
#include "arena.h"
#include "mem.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern Shape *_g_root;
#define MARK_OBJECT_END 200
#define MARK_ARRAY_END 201
#define MAX_JSON_DEPTH 64

#define PUSH(stack, value) stack_push((stack), &(typeof(value)){(value)})
#define PEEK_INT(stack, offset)                                                \
  (*(int *)stack_peek((stack), (stack)->top - (offset)))

// Grammar Rules (Non-Terminals)
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
static void print_token(TokenType type);
static void link_node_to_scope(Stack *nodes, Stack *scopes, int new_node_idx,
                               set_t *set, KeyTreePool *key_pool);

void (*unused_fn)(TokenType t) = print_token;
static void recursive_path_builder(ASTNode *pool, int node_idx, char **cursor,
                                   char *end);

extern const Except MALFORMED_JSON;
extern const Except MAXIMUM_NESTED_DEPTH_REACHED;

static void print_token(TokenType type) {
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
  /*#region*/
  ASTNode node;
  node.type = type;
  node.token = t;
  node.first_child = -1;
  node.next_sibling = -1;
  node.parent = -1;
  node.key_tree_root = -1; // Initialize tree root to -1
  return node;
  /*#endregion*/
}

// Links a newly created node to the current active scope
static void link_node_to_scope(Stack *nodes, Stack *scopes, int new_node_idx,
                               set_t *set, KeyTreePool *key_pool) {
  /*#region*/
  (void)(key_pool);
  // Scope Stack Layout (Top to Bottom):
  // [0] Count
  // [1] Tail Index (Last Sibling)
  // [2] Parent Index
  if (scopes->top < 2)
    return; // Root level, no parent

  int count = PEEK_INT(scopes, 0);
  int tail = PEEK_INT(scopes, 1);
  int parent = PEEK_INT(scopes, 2);

  ASTNode *pool = (ASTNode *)nodes->data;
  pool[new_node_idx].parent = parent;

  // 1. Link Structure
  if (tail == -1) {
    pool[parent].first_child = new_node_idx;
  } else {
    pool[tail].next_sibling = new_node_idx;
  }

  // 1b. Value Skipping Logic
  // We only propagate the SKIPPED status if we are the VALUE in a Key-Value
  // pair (odd count), and our corresponding KEY (tail) was skipped.
  if (pool[parent].token.type == T_BRACE_OPEN && (count % 2) != 0) {
    if (tail != -1 && pool[tail].type == AST_SKIPPED) {
      pool[new_node_idx].type = AST_SKIPPED;
    }
  }

  // 2. Update Scope (Tail = new node, Count++)
  *(int *)stack_peek(scopes, scopes->top - 1) = new_node_idx;
  *(int *)stack_peek(scopes, scopes->top) = count + 1;

  // 3. Duplicate Detection and Key Sorting
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
          scoped_key[total_needed] = '\0'; // Fix: Ensure null-termination

          int result = set_insert(set, scoped_key, (uint16_t)total_needed);

          if (result == SET_KEY_ALREADY_EXIST) {
            pool[new_node_idx].type = AST_SKIPPED;
          } else {
            // Valid key! Insert it into the alphabetical Key Tree for this
            // object
            pool[parent].key_tree_root = key_tree_insert(
                key_pool, pool[parent].key_tree_root, new_node_idx, pool);
          }
        }
      }
    }
  }
  /*#endregion*/
}

static void recursive_path_builder(ASTNode *pool, int node_idx, char **cursor,
                                   char *end) {
  /*#region*/
  if (node_idx == -1)
    return;

  int p_idx = pool[node_idx].parent;

  if (p_idx == -1) {
    if (*cursor < end)
      *(*cursor)++ = '$';
    return;
  }

  recursive_path_builder(pool, p_idx, cursor, end);

  ASTNode *p = &pool[p_idx];

  if (p->type == AST_OBJECT) {
    int k = p->first_child;
    while (k != -1) {
      if (pool[k].type == AST_SKIPPED) {
        // Skip this pair entirely
        int v = pool[k].next_sibling;
        if (v == -1)
          break;
        k = pool[v].next_sibling;
        continue;
      }

      int v = pool[k].next_sibling;
      if (v == node_idx) {
        int len = snprintf(*cursor, end - *cursor, ".%.*s",
                           (int)pool[k].token.value.string.length,
                           pool[k].token.value.string.start);
        if (len > 0)
          *cursor += len;
        return;
      }
      if (v == -1)
        break;
      k = pool[v].next_sibling;
    }
    // Is node_idx the key itself?
    k = p->first_child;
    while (k != -1) {
      if (pool[k].type == AST_SKIPPED) {
        int v = pool[k].next_sibling;
        if (v == -1)
          break;
        k = pool[v].next_sibling;
        continue;
      }
      if (k == node_idx) {
        int len = snprintf(*cursor, end - *cursor, ".%.*s",
                           (int)pool[k].token.value.string.length,
                           pool[k].token.value.string.start);
        if (len > 0)
          *cursor += len;
        return;
      }
      int v = pool[k].next_sibling;
      if (v == -1)
        break;
      k = pool[v].next_sibling;
    }

  } else if (p->type == AST_ARRAY) {
    int idx = 0;
    int k = p->first_child;
    while (k != -1) {
      if (k == node_idx) {
        int len = snprintf(*cursor, end - *cursor, "[%d]", idx);
        if (len > 0)
          *cursor += len;
        return;
      }
      k = pool[k].next_sibling;
      idx++;
    }
  }
  /*#endregion*/
}

void jsonv_ast(Lexer *lexer, Stack *nodes, Stack *scopes, Stack *controls,
               set_t *set, KeyTreePool *key_pool) {
  /*#region*/
  nodes->top = -1;
  scopes->top = -1;
  controls->top = -1;
  int depth = 0;

  PUSH(controls, RULE_JSON);
  Token c = lexer_next_token(lexer);
  Token n = lexer_next_token(lexer);

  while (controls->top >= 0) {
    int expected = *(int *)stack_pop(controls);

    // --- MATCHING TERMINALS ---
    if (expected < RULE_JSON) {
      if (expected != (int)c.type)
        RAISE(MALFORMED_JSON);

      // Handle Leaf Nodes (Strings, Numbers, etc.)
      if (c.type == T_STRING || c.type == T_NUMBER || c.type == T_TRUE ||
          c.type == T_FALSE || c.type == T_NULL) {
        ASTNode leaf = new_node(AST_LEAF, c);
        stack_push(nodes, &leaf);
        link_node_to_scope(nodes, scopes, nodes->top, set, key_pool);
      }

      c = n;
      n = lexer_next_token(lexer);

      if (n.type == T_EOF && controls->top == -1) {
        break;
      }
      continue;
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
        RAISE(MALFORMED_JSON);
      break;

    case RULE_OBJECT: {
      depth++;
      if (depth > MAX_JSON_DEPTH)
        RAISE(MAXIMUM_NESTED_DEPTH_REACHED);

      // 1. Create Object Node IMMEDIATELY
      ASTNode obj_node = new_node(AST_OBJECT, c);
      stack_push(nodes, &obj_node);
      int obj_idx = nodes->top;

      // 2. Link to parent (if any)
      link_node_to_scope(nodes, scopes, obj_idx, set, key_pool);

      // 3. Push new Scope [Parent, Tail, Count]
      PUSH(scopes, obj_idx); // Parent
      PUSH(scopes, -1);      // Tail (initially none)
      PUSH(scopes, 0);       // Count

      PUSH(controls, MARK_OBJECT_END);
      PUSH(controls, T_BRACE_CLOSE);

      if (n.type != T_BRACE_CLOSE)
        PUSH(controls, RULE_MEMBERS);
      PUSH(controls, T_BRACE_OPEN);
      break;
    }

    case RULE_ARRAY: {
      depth++;
      if (depth > MAX_JSON_DEPTH)
        RAISE(MAXIMUM_NESTED_DEPTH_REACHED);

      // 1. Create Array Node IMMEDIATELY
      ASTNode arr_node = new_node(AST_ARRAY, c);
      stack_push(nodes, &arr_node);
      int arr_idx = nodes->top;

      // 2. Link to parent
      link_node_to_scope(nodes, scopes, arr_idx, set, key_pool);

      // 3. Push new Scope
      PUSH(scopes, arr_idx); // Parent
      PUSH(scopes, -1);      // Tail (initially none)
      PUSH(scopes, 0);       // Count

      PUSH(controls, MARK_ARRAY_END);
      PUSH(controls, T_BRACKET_CLOSE);

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
    case MARK_OBJECT_END:
    case MARK_ARRAY_END: {
      stack_pop(scopes); // Pop Count
      stack_pop(scopes); // Pop Tail
      stack_pop(scopes); // Pop Parent
      depth--;
      break;
    }
    }
  }

  if (n.type != T_EOF)
    RAISE(MALFORMED_JSON);
  /*#endregion*/
}

void print_ast(Stack *nodes, int index, int indent) {
  /*#region*/
  ASTNode *pool = (ASTNode *)nodes->data;
  int curr = index;

  // Use iterative loop for siblings to prevent stack overflow
  while (curr != -1) {
    ASTNode *node = &pool[curr];

    if (node->type == AST_SKIPPED) {
      curr = node->next_sibling;
      continue;
    }

    for (int i = 0; i < indent; i++)
      printf("  ");

    switch ((int)node->type) {
    case AST_OBJECT:
      printf("OBJECT {\n");
      // Recurse for children (nested depth is safe, sibling depth is not)
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
        printf("LEAF: %f\n", node->token.value.number);
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
        printf("LEAF: %.*s\n", (int)node->token.value.string.length,
               node->token.value.string.start);
      }
      break;
    }
    }

    curr = node->next_sibling;
  }
  /*#endregion*/
}

int jsonv_find_property(ASTNode *json_pool, int object_idx, const char *key) {
  /*#region*/
  ASTNode *obj = &json_pool[object_idx];
  if (obj->type != AST_OBJECT)
    return -1;

  size_t key_len = strlen(key);
  int curr = obj->first_child;
  while (curr != -1) {
    if (json_pool[curr].type != AST_SKIPPED) {
      Token k = json_pool[curr].token;
      if (k.value.string.length == key_len &&
          strncmp((char *)k.value.string.start, key, key_len) == 0) {
        return json_pool[curr].next_sibling;
      }
    }
    int val_idx = json_pool[curr].next_sibling;
    if (val_idx == -1)
      break;
    curr = json_pool[val_idx].next_sibling;
  }
  return -1;
  /*#endregion*/
}

char *get_node_path(Stack *nodes, int node_idx, size_t size) {
  /*#region*/
  assert(size > 0);
  if (node_idx == -1)
    return NULL;
  ASTNode *pool = (ASTNode *)nodes->data;

  char *buffer = ALLOC(size);
  if (!buffer)
    return NULL;

  char *cursor = buffer;
  char *end = buffer + size;

  recursive_path_builder(pool, node_idx, &cursor, end);
  *cursor = '\0';

  return buffer;
  /*#endregion*/
}

static void print_indent(int ind) {
/*#region*/
  for (int i = 0; i < ind; i++)
    printf("  ");
/*#endregion*/
}

void print_ast_alphabetical(ASTNode *pool, KeyTreePool *key_pool, int node_idx,
                            int indent) {
  /*#region*/
  if (node_idx == -1)
    return;

  ASTNode *node = &pool[node_idx];

  // Skip nodes that were marked as duplicates
  if (node->type == AST_SKIPPED)
    return;

  print_indent(indent);

  switch ((int)node->type) {
  case AST_OBJECT: {
    printf("OBJECT {\n");

    // Use a local array to retrieve the sorted keys.
    // (Assumes a max of 256 keys per object. For production, you could
    // dynamically allocate this based on object size).
    int sorted_keys[256];
    int count = 0;

    // Extract AST Key node indices in strict alphabetical order
    key_tree_get_ordered(key_pool, node->key_tree_root, sorted_keys, &count);

    for (int i = 0; i < count; i++) {
      int key_idx = sorted_keys[i];
      ASTNode *key_node = &pool[key_idx];

      // 1. Print the Key
      print_indent(indent + 1);
      printf("KEY(%.*s): ", (int)key_node->token.value.string.length,
             key_node->token.value.string.start);

      // 2. The Value is always the immediate next sibling of the Key in our AST
      // layout
      int val_idx = key_node->next_sibling;

      if (val_idx != -1) {
        // We print the value inline if it's a leaf, or on a new line if it's a
        // complex structure
        if (pool[val_idx].type == AST_LEAF) {
          print_ast_alphabetical(pool, key_pool, val_idx,
                                 0); // 0 indent to print on same line
        } else {
          printf("\n");
          print_ast_alphabetical(pool, key_pool, val_idx, indent + 2);
        }
      }
    }

    print_indent(indent);
    printf("}\n");
    break;
  }

  case AST_ARRAY: {
    printf("ARRAY [\n");
    // Arrays keep their original parsed order
    int child_idx = node->first_child;
    while (child_idx != -1) {
      print_ast_alphabetical(pool, key_pool, child_idx, indent + 1);
      child_idx = pool[child_idx].next_sibling;
    }
    print_indent(indent);
    printf("]\n");
    break;
  }

  case AST_LEAF: {
    switch (node->token.type) {
    case T_NUMBER:
      printf("%f\n", node->token.value.number);
      break;
    case T_NULL:
      printf("null\n");
      break;
    case T_TRUE:
      printf("true\n");
      break;
    case T_FALSE:
      printf("false\n");
      break;
    case T_STRING:
    default:
      printf("\"%.*s\"\n", (int)node->token.value.string.length,
             node->token.value.string.start);
      break;
    }
    break;
  }
  }
  /*#endregion*/
}

static lstr_t arena_alloc_str(Arena *arena, const unsigned char *start, size_t len) {
  /*#region*/
  // Safe overflow check
  if (len > SIZE_MAX - sizeof(StringHeader) - 1) {
    return NULL;
  }
  size_t total_size = sizeof(StringHeader) + len + 1;
  StringHeader *str = (StringHeader *)arena_alloc(arena, total_size);
  if (!str) {
    return NULL;
  }
  str->length = (uint32_t)len;
  memcpy(str->data, start, len);
  str->data[len] = '\0';
  return (lstr_t)str->data;
  /*#endregion*/
}

// Recursively converts an AST node into a runtime Value using chained arena allocations
Value ast_to_value(ASTNode *pool, KeyTreePool *key_pool, int node_idx, Shape *shape_root, Arena *arena) {
    if (node_idx == -1) return val_null();
    
    ASTNode *node = &pool[node_idx];

    switch ((int)node->type) {
        case AST_OBJECT: {
            // Create the runtime object starting from the root shape
            Obj *obj = obj_new(shape_root);
            
            // We use a local array to retrieve the sorted keys.
            // 256 is used for safety, but you can dynamically allocate this if needed.
            #define MAX_OBJ_KEYS 256
            int sorted_keys[MAX_OBJ_KEYS];
            int count = 0;
            
            // Extract AST Key node indices in strict alphabetical order
            key_tree_get_ordered(key_pool, node->key_tree_root, sorted_keys, &count);

            for (int i = 0; i < count; i++) {
                int key_idx = sorted_keys[i];
                ASTNode *key_node = &pool[key_idx];
                int val_idx = key_node->next_sibling;
                
                if (val_idx != -1) {
                    // 1. Evaluate the child value recursively
                    Value child_val = ast_to_value(pool, key_pool, val_idx, shape_root, arena);
                    
                    // 2. Extract the key string dynamically using length-prefixed Arena allocation
                    int klen = key_node->token.value.string.length;
                    lstr_t key_str = arena_alloc_str(arena, key_node->token.value.string.start, klen);
                    
                    // 3. Set the property on the object
                    // Because we iterate through sorted_keys, obj_set is ALWAYS 
                    // called in alphabetical order, maximizing shape sharing!
                    obj_set(obj, key_str, child_val);
                }
            }
            
            return val_obj(obj);
        }

        case AST_ARRAY: {
            // Assumes you have an array implementation in your runtime
            Arr *arr = arr_new();
            
            int child_idx = node->first_child;
            for (int i = 0; child_idx != -1; i++) {
                // Arrays maintain their parsed order
                Value child_val = ast_to_value(pool, key_pool, child_idx, shape_root, arena);
                arr_set(arr, i, child_val);
                child_idx = pool[child_idx].next_sibling;
            }
            
            return val_arr(arr);
        }

        case AST_LEAF: {
            switch (node->token.type) {
                case T_NUMBER:
                    // You might want to check if it's an integer vs float here
                    // depending on how your JSON parser stores numbers
                    return val_double(node->token.value.number);
                case T_NULL:
                    return val_null();
                case T_TRUE:
                    return val_bool(1);
                case T_FALSE:
                    return val_bool(0);
                case T_STRING: {
                    // Extract null-terminated string using length-prefixed Arena allocation
                    int slen = node->token.value.string.length;
                    lstr_t tmp_str = arena_alloc_str(arena, node->token.value.string.start, slen);
                    
                    Value str_val = val_str(tmp_str);
                    return str_val;
                }
                default:
                    return val_null();
            }
        }
        
        default:
            return val_null();
    }
}
