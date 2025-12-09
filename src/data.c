#include "data.h"
#include "assert.h"
#include "compile.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static jsonv_t get_type(const char *json, jsmntok_t *token) {
  /*#region*/
  if (token->type == JSMN_PRIMITIVE) {
    switch ((json + token->start)[0]) {
    case 't':
      return jsonv_BOOLEAN;
    case 'f':
      return jsonv_BOOLEAN;
    case 'n':
      return jsonv_NULL;
    default:
      return jsonv_NUMBER;
    }
  }
  switch (token->type) {
  case JSMN_OBJECT:
    return jsonv_OBJECT;
  case JSMN_ARRAY:
    return jsonv_ARRAY;
  case JSMN_STRING:
    return jsonv_STRING;
  default:
    return jsonv_UNKNOWN;
  }
  /*#endregion*/
}

static void cleanup_node_internals(Jsonv_DataNode *node) {
  /*#region*/
  assert(node);
  if (node->properties != NULL) {
    for (size_t i = 0; i < node->property_count; i++) {
      cleanup_node_internals(node->properties[i]);
    }
    free(node->properties);
    node->properties = NULL;
  }
  /*#endregion*/
}

Jsonv_DataNode *jsonv_compile_data(const char *json, jsonv_tokiterator *it,
                                   Jsonv_DataNode *parent, Jsonv_path *path,
                                   Jsonv_error_stack *errors){
/*#region*/
  jsmntok_t *tok = jsonv_tokiterator_current(it);
  if (!tok)
    return NULL;

  Jsonv_DataNode *node = calloc(1, sizeof(Jsonv_DataNode));
  if (!node)
    return NULL;

  node->parent = parent;
  node->type = get_type(json, tok);

  /* ----- CASE 1: PRIMITIVE OR STRING ----- */
  if (tok->type == JSMN_PRIMITIVE || tok->type == JSMN_STRING) {
    node->value.start = tok->start;
    node->value.end = tok->end;
    return node;
  }

  /* ----- CASE 2: OBJECT / ARRAY ----- */
  int count = tok->size;
  node->property_count = count;

  if (count == 0)
    return node;

  /* allocate array of pointers (NOT structs!) */
  node->properties = calloc(count, sizeof(Jsonv_DataNode *));
  if (!node->properties) {
    free(node);
    return NULL;
  }

  for (int i = 0; i < count; i++) {

    jsonv_tokiterator_next(it);

    Jsonv_DataNode *child = NULL;
    jsmntok_t *keytok = NULL;

    /* --- OBJECT ENTRY: read key first --- */
    if (tok->type == JSMN_OBJECT) {
      keytok = jsonv_tokiterator_current(it);

      if (!keytok || keytok->type != JSMN_STRING) {
        fprintf(stderr, "Malformed object: expected key.\n");
        node->property_count = i;
        return node;
      }

      /* store key in a temporary node */
      jsonv_Hint keyhint = {.start = keytok->start, .end = keytok->end};

      /* advance to the value token */
      jsonv_tokiterator_next(it);

      child = jsonv_compile_data(json, it, node, path, errors);
      if (!child) {
        fprintf(stderr, "Failed to compile child.\n");
        node->property_count = i;
        return node;
      }

      /* assign key after child exists */
      child->key = keyhint;
    }

    /* --- ARRAY ENTRY: no key, just a value --- */
    else if (tok->type == JSMN_ARRAY) {
      child = jsonv_compile_data(json, it, node, path, errors);

      if (!child) {
        fprintf(stderr, "Failed to compile child.\n");
        node->property_count = i;
        return node;
      }

      /* array child uses key = {-1,-1} */
      child->key.start = -1;
      child->key.end = -1;
    }

    node->properties[i] = child;
  }

  return node;
/*#endregion*/
}

void jsonv_data_free(Jsonv_DataNode *node) {
  /*#region*/
  assert(node);
  cleanup_node_internals(node);
  free(node);
  /*#endregion*/
}

/*
 * Build JSON path for a node:
 *   object members → ".key"
 *   array members  → "[index]"
 *
 * Returned string must be free()'d by the caller.
 */
char *jsonv_build_data_path(const Jsonv_DataNode *node, const char *json) {
  if (!node)
    return strdup("$");

  // Temporary list of path segments
  char **segments = NULL;
  size_t seg_count = 0;

  const Jsonv_DataNode *cur = node;

  while (cur->parent != NULL) {
    const Jsonv_DataNode *parent = cur->parent;

    char buffer[256];

    if (parent->type == jsonv_OBJECT) {
      // Objects → use key
      char key[100] = {0};
      int len = cur->key.end - cur->key.start;
      strncpy(key, json + cur->key.start, len);
      snprintf(buffer, sizeof(buffer), ".%s", key);

    } else if (parent->type == jsonv_ARRAY) {
      // Arrays → find index of cur in parent->properties
      size_t index = 0;
      bool found = false;

      for (size_t i = 0; i < parent->property_count; ++i) {
        if (parent->properties[i] == cur) {
          index = i;
          found = true;
          break;
        }
      }

      if (!found)
        snprintf(buffer, sizeof(buffer), "[?]");
      else
        snprintf(buffer, sizeof(buffer), "[%zu]", index);

    } else {
      snprintf(buffer, sizeof(buffer), ".?");
    }

    // Push segment
    segments = realloc(segments, sizeof(char *) * (seg_count + 1));
    segments[seg_count++] = strdup(buffer);

    cur = parent;
  }

  // Compute final length
  size_t length = 2; // for `$` and '\0'
  for (size_t i = 0; i < seg_count; i++)
    length += strlen(segments[i]);

  char *path = malloc(length);
  strcpy(path, "$");

  // Add segments reversed
  for (size_t i = 0; i < seg_count; i++)
    strcat(path, segments[seg_count - 1 - i]);

  // Cleanup
  for (size_t i = 0; i < seg_count; i++)
    free(segments[i]);
  free(segments);

  return path;
}

