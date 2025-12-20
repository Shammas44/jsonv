#ifndef _JSONV_NODE_H_INCLUDED
#define _JSONV_NODE_H_INCLUDED
#include "error.h"
#include "token.h"
#include <stdbool.h>

// counting start at 0
#define JSONV_MAX_NESTED_DEPTH 2

typedef enum Jsonv_Node_t {
  JSONV_NODE_T_ROOT,
  JSONV_NODE_T_OBJECT,
  JSONV_NODE_T_ARRAY,
  JSONV_NODE_T_STRING,
  JSONV_NODE_T_NUMBER,
  JSONV_NODE_T_NULL,
  JSONV_NODE_T_BOOL,
  JSONV_NODE_T_UNKNOWN,
} Jsonv_Node_t;

typedef struct {
  char *start;
  char *end;
} Jsonv_Boundary;

typedef struct Jsonv_Node {
  Jsonv_Node_t type;
  Jsonv_Boundary key;
  int index;
  struct Jsonv_Node *parent;
} Jsonv_Node;

typedef struct Jsonv_Node_Container {
  Jsonv_Node_t type;
  Jsonv_Boundary key;
  int index;
  struct Jsonv_Node *parent;
  size_t depth;
  size_t length;
  Jsonv_Node **items;
} Jsonv_Node_Container;

typedef struct Jsonv_Node_String {
  Jsonv_Node_t type;
  Jsonv_Boundary key;
  int index;
  struct Jsonv_Node *parent;
  Jsonv_Boundary value;
  size_t length;
} Jsonv_Node_String;

typedef struct Jsonv_Node_Number {
  Jsonv_Node_t type;
  Jsonv_Boundary key;
  int index;
  struct Jsonv_Node *parent;
  double value;
} Jsonv_Node_Number;

typedef struct Jsonv_Node_Null {
  Jsonv_Node_t type;
  Jsonv_Boundary key;
  int index;
  struct Jsonv_Node *parent;
} Jsonv_Node_Null;

typedef struct Jsonv_Node_Bool {
  Jsonv_Node_t type;
  Jsonv_Boundary key;
  int index;
  struct Jsonv_Node *parent;
  bool value;
} Jsonv_Node_Bool;

typedef struct Jsonv_Node_Unknown {
  Jsonv_Node_t type;
  Jsonv_Boundary key;
  int index;
  struct Jsonv_Node *parent;
} Jsonv_Node_Unknown;

Jsonv_Node *jsonv_compile_node(const char *json, jsonv_tokiterator *it,
                               Jsonv_Node *parent, Jsonv_path *path,
                               Jsonv_error_stack *errors, size_t index);

void jsonv_node_free(Jsonv_Node *node);

#endif
