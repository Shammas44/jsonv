#include "print.h"
#include "assert.h"
#include "node.h"
#include "schema.h"
#include <stdio.h>
#include <string.h>

static const char *schema_type_to_string(jsonv_t type) {
  /*#region*/
  switch (type) {
  case jsonv_STRING:
    return "string";
  case jsonv_NUMBER:
    return "number";
  case jsonv_INTEGER:
    return "integer";
  case jsonv_BOOLEAN:
    return "boolean";
  case jsonv_OBJECT:
    return "object";
  case jsonv_ARRAY:
    return "array";
  case jsonv_UNKNOWN:
  default:
    return "unknown";
  }
  /*#endregion*/
}

static void print_indent(int depth) {
  /*#region*/
  for (int i = 0; i < depth; i++) {
    printf("  "); // Two spaces per level
  }
  /*#endregion*/
}

static void print_hint_internal(const char *json, jsonv_Hint hint,
                                int max_len) {
  /*#region*/
  if (hint.start >= 0 && hint.end > hint.start) {
    int len = hint.end - hint.start;
    int print_len = (len < max_len) ? len : max_len;
    printf("%.*s", print_len, json + hint.start);
    if (len > max_len)
      printf("...");
  }
  /*#endregion*/
}

void jsonv_print_schema_internal(const Jsonv_SchemaNode *schema, int depth,
                                 const char *json) {
  /*#region*/
  if (!schema) {
    print_indent(depth);
    printf("[NULL JSON SCHEMA]\n");
    return;
  }

  // 1. Print current node type and basic constraints
  print_indent(depth);
  printf("Type: %s", schema_type_to_string(schema->type));

  // print contraintes
  List list = schema->value_contraints;
  if (list_length(list) > 0) {
    for (; list; list = list->rest) {
      Jsonv_Contraint *c = list->first;
      char buff[100] = {0};
      HINT(json, c->name, buff);
      puts("");
      print_indent(depth);
      printf("[%s]", buff);
    }
  }
  printf("\n");

  // 2. Handle Object Type (Recursive Step 1)
  if (schema->type == jsonv_OBJECT) {
    print_indent(depth);
    printf("Properties (%zu, additional: %s):\n", schema->property_count,
           schema->additional_properties ? "true" : "false");

    for (size_t i = 0; i < schema->property_count; i++) {
      const Jsonv_SchemaNode *prop = &schema->properties[i];
      print_indent(depth + 1);
      int len = prop->key.end - prop->key.start;
      char key[256] = {0};
      strncpy(key, json + prop->key.start, len);
      printf("Key: '%s' (Required: %s)\n", key, prop->required ? "YES" : "NO");
      jsonv_print_schema_internal(prop, depth + 2, json);
    }
  }

  // 3. Handle Array Type (Recursive Step 2)
  else if (schema->type == jsonv_ARRAY) {
    print_indent(depth);
    printf("Items Schema:\n");
    const Jsonv_SchemaNode *prop = schema->items;
    jsonv_print_schema_internal(prop, depth + 1, json);
  }
  /*#endregion*/
}

void jsonv_print_data_internal(const Jsonv_DataNode *node, int depth,
                               const char *json) {
  /*#region*/
  if (!node)
    return;

  // Determine if this node is an array item (has no key hint, and is not the
  // root node)
  int is_array_item = (depth > 0 && node->key.start < 0);

  // 1. Print indentation
  print_indent(depth > 0 ? depth - 1 : depth);

  // 2. Print prefix (key: or - ) and a space.
  if (node->key.start > 0) {
    // Object property: print key followed by colon and space (YAML style)
    print_hint_internal(json, node->key, 40);
    printf(": ");
  } else if (is_array_item) {
    // Array item: print dash and space (YAML style)
    printf("- ");
  }

  // 3. Print the node's value or structural newline/recursion
  jsonv_t type = node->type;
  (void)(type);
  size_t k = node->property_count;
  (void)(k);

  switch (node->type) {
  case jsonv_OBJECT:
  case jsonv_ARRAY:
    // If the node is a container (object or array), we print a newline
    // to move the children to the next indented line.
    // Note: The prefix (key: / - ) already included the space/newline setup.
    printf("\n");

    // Recurse for children, increasing depth by 1
    // Iterating over the contiguous array of DataNode structs pointed to by
    // properties
    for (size_t i = 0; i < node->property_count; i++) {
      jsonv_print_data_internal(node->properties[i], depth + 1, json);
    }
    break;

  case jsonv_STRING:
    // Primitive/String: Print with single quotes.
    printf("'");
    print_hint_internal(json, node->value, 40);
    printf("'\n");
    break;

  case jsonv_NUMBER:
  case jsonv_BOOLEAN:
  case jsonv_NULL:
    // Primitive: Print without quotes.
    print_hint_internal(json, node->value, 40);
    printf("\n");
    break;

  default:
    printf(" (UNKNOWN TYPE)\n");
    break;
  }
  /*#endregion*/
}

static void print_hint_node_internal(Jsonv_Boundary boundary, int max_len) {
  /*#region*/
  if (boundary.end > boundary.start) {
    int len = boundary.end - boundary.start;
    int print_len = (len < max_len) ? len : max_len;
    printf("%.*s", print_len, boundary.start);
    if (len > max_len)
      printf("...");
  }
  /*#endregion*/
}

void jsonv_print_node_internal(const Jsonv_Node *node, int depth,
                               const char *json) {
  /*#region*/
  assert(node);
  assert(json);
  bool is_array_item = node->parent && node->parent->type == JSONV_NODE_T_ARRAY;

  // 1. Print indentation
  print_indent(depth > 0 ? depth - 1 : depth);

  // 2. Print prefix (key: or - ) and a space.
  if (is_array_item) {
    printf("- ");
  } else if (node->parent && node->parent->type == JSONV_NODE_T_OBJECT) {
    print_hint_node_internal(node->key, 40);
    printf(": ");
  } else if (node->type == JSONV_NODE_T_OBJECT &&
             ((Jsonv_Node_Container *)node)->length == 0) {
    print_hint_node_internal(node->key, 40);
    printf(": {}");
  } else if (node->type == JSONV_NODE_T_ARRAY &&
             ((Jsonv_Node_Container *)node)->length == 0) {
    print_hint_node_internal(node->key, 40);
    printf(": []");
  } else if (node->parent && node->parent->type == JSONV_NODE_T_ROOT) {
    print_hint_node_internal(node->key, 40);
    printf(": ");
  }
  // 3. Print the node's value or structural newline/recursion
  switch (node->type) {
  case JSONV_NODE_T_ROOT:
    printf("\n");
    Jsonv_Node_Container *c = (Jsonv_Node_Container *)node;
    for (size_t i = 0; i < c->length; i++) {
      jsonv_print_node_internal(c->items[i], depth + 1, json);
    }
    break;
  case JSONV_NODE_T_OBJECT:
    printf("(depth %zu)\n", ((Jsonv_Node_Container *)node)->depth);
    Jsonv_Node_Container *c1 = (Jsonv_Node_Container *)node;
    for (size_t i = 0; i < c1->length; i++) {
      jsonv_print_node_internal(c1->items[i], depth + 1, json);
    }
    break;
  case JSONV_NODE_T_ARRAY:
    printf("(depth %zu)\n", ((Jsonv_Node_Container *)node)->depth);
    Jsonv_Node_Container *c2 = (Jsonv_Node_Container *)node;
    for (size_t i = 0; i < c2->length; i++) {
      jsonv_print_node_internal(c2->items[i], depth + 1, json);
    }
    break;

  case JSONV_NODE_T_STRING:
    printf("'");
    print_hint_node_internal(((Jsonv_Node_String *)node)->value, 40);
    printf("'\n");
    break;

  case JSONV_NODE_T_NUMBER:
    printf("%f\n", ((Jsonv_Node_Number *)node)->value);
    break;
  case JSONV_NODE_T_BOOL:
    printf("%s\n", ((Jsonv_Node_Bool *)node)->value ? "true" : "false");
    break;
  case JSONV_NODE_T_NULL:
    printf("null\n");
    break;

  default:
    printf(" (UNKNOWN TYPE)\n");
    break;
  }
  /*#endregion*/
}
