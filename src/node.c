#include "node.h"
#include "assert.h"
#include "atom.h"
#include "error.h"
#include "hint.h"
#include "lexer.h"
#include "mem.h"
#include "number.h"
#include "set.h"
#include "string.h"
#include "token.h"
#include <stdio.h>
#include <stdlib.h>

extern const Except MALFORMED_JSON;
extern const Except MAXIMUM_NESTED_DEPTH_REACHED;

// static char *jsonv_build_node_path(const Jsonv_Node *node);
// static Jsonv_Node_t get_type(const char *json, jsmntok_t *token) {
//   /*#region*/
//   if (token->type == JSMN_PRIMITIVE) {
//     int len = token->end - token->start;
//     Jsonv_Node_t type;
//     char *buff = malloc(sizeof(char) * len);
//     strncpy(buff, json + token->start, len);
//     double out;

//     switch ((json + token->start)[0]) {
//     case 't':
//       type =
//           strcmp(buff, "true") == 0 ? JSONV_NODE_T_BOOL :
//           JSONV_NODE_T_UNKNOWN;
//       break;
//     case 'f':
//       type =
//           strcmp(buff, "false") == 0 ? JSONV_NODE_T_BOOL :
//           JSONV_NODE_T_UNKNOWN;
//       break;
//     case 'n':
//       type =
//           strcmp(buff, "null") == 0 ? JSONV_NODE_T_BOOL :
//           JSONV_NODE_T_UNKNOWN;
//       break;
//     default:
//       type =
//           parse_double(buff, &out) ? JSONV_NODE_T_NUMBER :
//           JSONV_NODE_T_UNKNOWN;
//     }
//     free(buff);
//     return type;
//   }
//   switch (token->type) {
//   case JSMN_OBJECT:
//     return JSONV_NODE_T_OBJECT;
//   case JSMN_ARRAY:
//     return JSONV_NODE_T_ARRAY;
//   case JSMN_STRING:
//     return JSONV_NODE_T_STRING;
//   default:
//     return JSONV_NODE_T_UNKNOWN;
//   }
//   /*#endregion*/
// }

static void cleanup_node_internals(Jsonv_Node *node) {
  /*#region*/
  assert(node);
  // if (node->type == JSONV_NODE_T_OBJECT || node->type == JSONV_NODE_T_ROOT ||
  //     node->type == JSONV_NODE_T_ARRAY) {
  //   Jsonv_Node_Container *container = (Jsonv_Node_Container *)node;
  //   int len = container->length;
  //   for (int i = 0; i < len; i++) {
  //     cleanup_node_internals(container->items[i]);
  //   }
  //   free(container->items);
  //   if (node->type != JSONV_NODE_T_ROOT) {
  //     free(node);
  //   }
  // } else {
  //   free(node);
  // }
  /*#endregion*/
}

// static Jsonv_Node *handle_object(const char *json, Lexer *it,
//                                  Jsonv_Node *parent, Jsonv_path *path,
//                                  Jsonv_error_stack *errors, size_t index) {
//   /*#region*/
//   Token tok;
//   tok = lexer_next_token(it);
//   assert(tok.type == T_STRING);
//   tok = lexer_next_token(it);
//   assert(tok.type == T_COLON);
//   tok = lexer_next_token(it);
//   Jsonv_Node_Container *p = (Jsonv_Node_Container *)parent;
//   Jsonv_Node *child =
//       jsonv_compile_node(json, it, (Jsonv_Node *)p, path, errors, index);
//   if (p->items)
//     list_push(p->items, child);
//   if (!p->items)
//     p->items = list_new(child, NULL);
//   tok = lexer_next_token(it);
//   if (tok.type != T_COMMA) {
//     tok = lexer_next_token(it);
//     assert(tok.type == T_BRACE_CLOSE);
//   }
//   return child;
//   /*#endregion*/
// }

Jsonv_Node *jsonv_compile_node(const char *json, Lexer *it, Jsonv_Node *parent,
                               Jsonv_path *path, Jsonv_error_stack *errors,
                               size_t index) {
  /*#region*/
  assert(json);
  assert(it);
  assert(path);
  assert(errors);
  assert(index);
  assert(parent);
  return NULL;

  // if (type == JSONV_NODE_T_ARRAY) {
  //   jsmntok_t *key_tok = jsonv_tokiterator_relative(it, -1);
  //   n->key.start = (char *)json + key_tok->start;
  //   n->key.end = (char *)json + key_tok->end;
  //   n->depth = ((Jsonv_Node_Container *)parent)->depth + 1;

  //   if (n->depth == JSONV_MAX_NESTED_DEPTH) {
  //     // RAISE FOR: "MAXIMUM NESTED DEPTH REACHED"
  //     RAISE(MAXIMUM_NESTED_DEPTH_REACHED);
  //   }

  //   for (int i = 0; i < count; i++) {
  //     jsonv_tokiterator_next(it);
  //     child = jsonv_compile_node(json, it, (Jsonv_Node *)n, path, errors, i);
  //     assert(child);
  //     n->items[i] = child;
  //   }
  // }

  // return (Jsonv_Node *)n;
  /*#endregion*/
}

void jsonv_node_free(Jsonv_Node *node) {
  /*#region*/
  assert(node);
  cleanup_node_internals(node);
  free(node);
  /*#endregion*/
}
