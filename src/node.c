#include "node.h"
#include "assert.h"
#include "atom.h"
#include "error.h"
#include "hint.h"
#include "mem.h"
#include "number.h"
#include "set.h"
#include "string.h"
#include <stdio.h>
#include <stdlib.h>

extern const Except MALFORMED_JSON;
extern const Except MAXIMUM_NESTED_DEPTH_REACHED;

// static char *jsonv_build_node_path(const Jsonv_Node *node);
static Jsonv_Node_t get_type(const char *json, jsmntok_t *token) {
  /*#region*/
  if (token->type == JSMN_PRIMITIVE) {
    int len = token->end - token->start;
    Jsonv_Node_t type;
    char *buff = malloc(sizeof(char) * len);
    strncpy(buff, json + token->start, len);
    double out;

    switch ((json + token->start)[0]) {
    case 't':
      type =
          strcmp(buff, "true") == 0 ? JSONV_NODE_T_BOOL : JSONV_NODE_T_UNKNOWN;
      break;
    case 'f':
      type =
          strcmp(buff, "false") == 0 ? JSONV_NODE_T_BOOL : JSONV_NODE_T_UNKNOWN;
      break;
    case 'n':
      type =
          strcmp(buff, "null") == 0 ? JSONV_NODE_T_BOOL : JSONV_NODE_T_UNKNOWN;
      break;
    default:
      type =
          parse_double(buff, &out) ? JSONV_NODE_T_NUMBER : JSONV_NODE_T_UNKNOWN;
    }
    free(buff);
    return type;
  }
  switch (token->type) {
  case JSMN_OBJECT:
    return JSONV_NODE_T_OBJECT;
  case JSMN_ARRAY:
    return JSONV_NODE_T_ARRAY;
  case JSMN_STRING:
    return JSONV_NODE_T_STRING;
  default:
    return JSONV_NODE_T_UNKNOWN;
  }
  /*#endregion*/
}

static void cleanup_node_internals(Jsonv_Node *node) {
  /*#region*/
  assert(node);
  if (node->type == JSONV_NODE_T_OBJECT || node->type == JSONV_NODE_T_ROOT ||
      node->type == JSONV_NODE_T_ARRAY) {
    Jsonv_Node_Container *container = (Jsonv_Node_Container *)node;
    int len = container->length;
    for (int i = 0; i < len; i++) {
      cleanup_node_internals(container->items[i]);
    }
    free(container->items);
    if (node->type != JSONV_NODE_T_ROOT) {
      free(node);
    }
  } else {
    free(node);
  }
  /*#endregion*/
}

Jsonv_Node *jsonv_compile_node(const char *json, jsonv_tokiterator *it,
                               Jsonv_Node *parent, Jsonv_path *path,
                               Jsonv_error_stack *errors, size_t index) {
  /*#region*/
  assert(json);
  assert(it);
  assert(path);
  assert(errors);
  jsmntok_t *tok = jsonv_tokiterator_current(it);
  assert(tok);
  Jsonv_Node_t type = get_type(json, tok);

  if (type == JSONV_NODE_T_UNKNOWN) {
    jsmntok_t *key = jsonv_tokiterator_relative(it, -1);
    Jsonv_Node_Unknown *node = CALLOC(1, sizeof(Jsonv_Node_Unknown));
    char buff[100] = {0};
    TOK(json, *tok, buff);
    node->index = index;
    node->type = JSONV_NODE_T_UNKNOWN;
    node->parent = parent;
    node->key.start = (char *)json + key->start;
    node->key.end = (char *)json + key->end;
    // RAISE FOR: "UNKNOWN NODE TYPE"
    RAISE(MALFORMED_JSON);
    return (Jsonv_Node *)node;
  }

  if (type == JSONV_NODE_T_STRING) {
    jsmntok_t *key = jsonv_tokiterator_relative(it, -1);
    Jsonv_Node_String *node = CALLOC(1, sizeof(Jsonv_Node_String));
    char buff[100] = {0};
    TOK(json, *tok, buff);
    node->index = index;
    node->value.start = (char *)json + tok->start;
    node->value.end = (char *)json + tok->end;
    node->type = JSONV_NODE_T_STRING;
    node->parent = parent;
    node->key.start = (char *)json + key->start;
    node->key.end = (char *)json + key->end;
    return (Jsonv_Node *)node;
  }

  if (type == JSONV_NODE_T_NUMBER) {
    jsmntok_t *key = jsonv_tokiterator_relative(it, -1);
    Jsonv_Node_Number *node = CALLOC(1, sizeof(Jsonv_Node_Number));
    char buff[100] = {0};
    TOK(json, *tok, buff);
    node->index = index;
    node->value = atof(buff);
    node->type = JSONV_NODE_T_NUMBER;
    node->parent = parent;
    node->key.start = (char *)json + key->start;
    node->key.end = (char *)json + key->end;
    return (Jsonv_Node *)node;
  }

  if (type == JSONV_NODE_T_BOOL) {
    jsmntok_t *key = jsonv_tokiterator_relative(it, -1);
    Jsonv_Node_Bool *node = CALLOC(1, sizeof(Jsonv_Node_Bool));
    char buff[100] = {0};
    TOK(json, *tok, buff);
    node->index = index;
    node->value = (char)(json + tok->start)[0] == 't' ? true : false;
    node->type = JSONV_NODE_T_BOOL;
    node->parent = parent;
    node->key.start = (char *)json + key->start;
    node->key.end = (char *)json + key->end;
    return (Jsonv_Node *)node;
  }

  if (type == JSONV_NODE_T_NULL) {
    jsmntok_t *key = jsonv_tokiterator_relative(it, -1);
    Jsonv_Node_Null *node = CALLOC(1, sizeof(Jsonv_Node_Null));
    node->index = index;
    node->type = JSONV_NODE_T_NULL;
    node->parent = parent;
    node->key.start = (char *)json + key->start;
    node->key.end = (char *)json + key->end;
    return (Jsonv_Node *)node;
  }

  if (parent->type == JSONV_NODE_T_ROOT &&
      ((Jsonv_Node_Container *)parent)->items == 0) {
    Jsonv_Node_Container *root = (Jsonv_Node_Container *)parent;
    jsmntok_t *open_bracket_tok = jsonv_tokiterator_current(it); // '{'
    root->key.start = (char *)json + open_bracket_tok->start;
    root->key.end = (char *)json + open_bracket_tok->end;
    root->depth = 0;
    int count = tok->size;
    Jsonv_Node *child = NULL;
    if (count > 0)
      root->items = CALLOC(count, sizeof(Jsonv_Node *));

    Set set = set_new(count, NULL, NULL);
    for (int i = 0; i < count; i++) {
      /* advance to the key token */
      jsmntok_t *keytok = jsonv_tokiterator_next(it);
      if (!keytok || keytok->type != JSMN_STRING) {
        // RAISE FOR: "MALFORMATED KEYS"
        RAISE(MALFORMED_JSON);
      }

      if (keytok->end - keytok->start == 0) {
        // RAISE FOR: "EMPTY STRING ARE NOT ALLOWED AS PROPERTY KEY"
        RAISE(MALFORMED_JSON);
      }

      if ((char)(json + keytok->start - 1)[0] != '"') {
        // RAISE FOR: "MALFORMATED KEYS"
        RAISE(MALFORMED_JSON);
      }
      if ((char)(json + keytok->end)[0] != '"') {
        // RAISE FOR: "MALFORMATED KEYS"
        RAISE(MALFORMED_JSON);
      }

      char buff[100] = {0};
      strncpy(buff, json + keytok->start, keytok->end - keytok->start);

      /* advance to the value token */
      jsonv_tokiterator_next(it);
      child = jsonv_compile_node(json, it, (Jsonv_Node *)root, path, errors, i);
      assert(child);

      int len = keytok->end - keytok->start;
      char *key_buff = ALLOC(len + 1);
      key_buff[len] = '\0';
      strncpy(key_buff, json + keytok->start, len);
      const char *atom = atom_string(key_buff);
      int isMember = set_member(set, atom);
      if (isMember) {
        // RAISE FOR: "PROPERTIES SHOULD BE UNIQUE"
        RAISE(MALFORMED_JSON);
      } else {
        set_put(set, atom);
      }

      child->key.start = (char *)json + keytok->start;
      child->key.end = (char *)json + keytok->end;
      root->items[i] = child;
    }
    set_free(&set);
    return (Jsonv_Node *)parent;
  }

  int count = tok->size;
  Jsonv_Node_Container *n = CALLOC(1, sizeof(Jsonv_Node_Container));
  n->length = count;
  n->parent = parent;
  n->type = tok->type == JSMN_OBJECT ? JSONV_NODE_T_OBJECT : JSONV_NODE_T_ARRAY;
  if (count > 0)
    n->items = CALLOC(count, sizeof(Jsonv_Node *));
  Jsonv_Node *child = NULL;

  if (type == JSONV_NODE_T_OBJECT) {
    jsmntok_t *keytok = jsonv_tokiterator_relative(it, -1);
    n->key.start = (char *)json + keytok->start;
    n->key.end = (char *)json + keytok->end;

    if ((char)(json + keytok->start - 1)[0] != '"') {
      // RAISE FOR: "MALFORMATED KEYS"
      RAISE(MALFORMED_JSON);
    }
    if ((char)(json + keytok->end)[0] != '"') {
      // RAISE FOR: "MALFORMATED KEYS"
      RAISE(MALFORMED_JSON);
    }

    n->depth = ((Jsonv_Node_Container *)parent)->depth + 1;

    if (n->depth == JSONV_MAX_NESTED_DEPTH) {
      // RAISE FOR: "MAXIMUM NESTED DEPTH REACHED"
      RAISE(MAXIMUM_NESTED_DEPTH_REACHED);
    }

    Set set = set_new(count, NULL, NULL);
    for (int i = 0; i < count; i++) {
      /* advance to the key token */
      jsmntok_t *keytok = jsonv_tokiterator_next(it);
      if (!keytok || keytok->type != JSMN_STRING) {
        // RAISE FOR: "MALFORMATED KEYS"
        RAISE(MALFORMED_JSON);
      }

      if (keytok->end - keytok->start == 0) {
        // RAISE FOR: "EMPTY STRING ARE NOT ALLOWED AS PROPERTY KEY"
        RAISE(MALFORMED_JSON);
      }

      char buff[100] = {0};
      strncpy(buff, json + keytok->start, keytok->end - keytok->start);

      /* advance to the value token */
      jsonv_tokiterator_next(it);
      child = jsonv_compile_node(json, it, (Jsonv_Node *)n, path, errors, i);
      assert(child);

      int len = keytok->end - keytok->start;
      char *key_buff = ALLOC(len + 1);
      key_buff[len] = '\0';
      strncpy(key_buff, json + keytok->start, len);
      const char *atom = atom_string(key_buff);
      int isMember = set_member(set, atom);
      if (isMember) {
        // RAISE FOR: "PROPERTIES SHOULD BE UNIQUE"
        RAISE(MALFORMED_JSON);
      } else {
        set_put(set, atom);
      }

      child->key.start = (char *)json + keytok->start;
      child->key.end = (char *)json + keytok->end;
      n->items[i] = child;
    }
    set_free(&set);
  }

  if (type == JSONV_NODE_T_ARRAY) {
    jsmntok_t *key_tok = jsonv_tokiterator_relative(it, -1);
    n->key.start = (char *)json + key_tok->start;
    n->key.end = (char *)json + key_tok->end;
    n->depth = ((Jsonv_Node_Container *)parent)->depth + 1;

    if (n->depth == JSONV_MAX_NESTED_DEPTH) {
      // RAISE FOR: "MAXIMUM NESTED DEPTH REACHED"
      RAISE(MAXIMUM_NESTED_DEPTH_REACHED);
    }

    for (int i = 0; i < count; i++) {
      jsonv_tokiterator_next(it);
      child = jsonv_compile_node(json, it, (Jsonv_Node *)n, path, errors, i);
      assert(child);
      n->items[i] = child;
    }
  }

  return (Jsonv_Node *)n;
  /*#endregion*/
}

void jsonv_node_free(Jsonv_Node *node) {
  /*#region*/
  assert(node);
  cleanup_node_internals(node);
  free(node);
  /*#endregion*/
}

// static char *jsonv_build_node_path(const Jsonv_Node *node) {
//   /*#region*/
//   if (!node)
//     return strdup("$");

//   // Temporary list of path segments
//   char **segments = NULL;
//   size_t seg_count = 0;

//   const Jsonv_Node_Container *cur = (Jsonv_Node_Container *)node;

//   while (cur->parent != NULL) {
//     const Jsonv_Node_Container *parent = (Jsonv_Node_Container *)cur->parent;

//     char buffer[256];

//     if (parent->type == JSONV_NODE_T_OBJECT ||
//         parent->type == JSONV_NODE_T_ROOT) {
//       // Objects → use key
//       char key[100] = {0};
//       int len = cur->key.end - cur->key.start;
//       strncpy(key, cur->key.start, len);
//       snprintf(buffer, sizeof(buffer), ".%s", key);

//     } else if (parent->type == JSONV_NODE_T_ARRAY) {
//       // Arrays → find index of cur in parent->properties
//       size_t index = 0;
//       bool found = false;

//       for (size_t i = 0; i < parent->length; ++i) {
//         if ((Jsonv_Node_Container *)parent->items[i] == cur) {
//           index = i;
//           found = true;
//           break;
//         }
//       }

//       if (!found)
//         snprintf(buffer, sizeof(buffer), "[?]");
//       else
//         snprintf(buffer, sizeof(buffer), "[%zu]", index);

//     } else {
//       snprintf(buffer, sizeof(buffer), ".?");
//     }

//     // Push segment
//     segments = realloc(segments, sizeof(char *) * (seg_count + 1));
//     segments[seg_count++] = strdup(buffer);

//     cur = parent;
//   }

//   // Compute final length
//   size_t length = 2; // for `$` and '\0'
//   for (size_t i = 0; i < seg_count; i++)
//     length += strlen(segments[i]);

//   char *path = malloc(length);
//   strcpy(path, "$");

//   // Add segments reversed
//   for (size_t i = 0; i < seg_count; i++)
//     strcat(path, segments[seg_count - 1 - i]);

//   // Cleanup
//   for (size_t i = 0; i < seg_count; i++)
//     free(segments[i]);
//   free(segments);

//   return path;
//   /*#endregion*/
// }
