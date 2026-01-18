#ifndef _JSONV_SCHEMA_H_INCLUDED
#define _JSONV_SCHEMA_H_INCLUDED
#include "stack.h"
#include "token.h"

// --- MACROS ---

// Use these to access nodes inside Stacks
#define SCH_NODE_FROM_STACK(s, i) (&((SchemaNode *)(s)->data)[i])
#define JSON_NODE_FROM_STACK(s, i) (&((ASTNode *)(s)->data)[i])

#define MAX_SCHEMA_NODES 2048
#define MAX_SCHEMA_CONTRAINTS 20

// 1. Grammar Commands
typedef enum {
  SC_CMD_BUILD_NODE,          // Main logic: Create node, parse attributes
  SC_CMD_BUILD_REQUIRED_NODE, //
  SC_CMD_CONNECT_PROPS,       // Post-logic: Link children to props_head
  SC_CMD_CONNECT_ITEMS,       // Post-logic: Link child to items_head
  SC_CMD_CONNECT_REQUIRED,    // Post-logic: Link strings to required_head
  SC_CMD_CONNECT_ADDITIONAL,  // Link schema to additional_schema
  SC_CMD_RETURN               // Finalize: Push result to result_stack
} SchemaCmdType;

// 2. Control Frame (What goes on the stack)
typedef struct {
  SchemaCmdType cmd;
  int json_idx;        // The input JSON node we are processing
  int schema_idx;      // The output Schema node we are operating on
  int count;           // Counter (e.g., how many properties to pop)
  Token name_override; // To pass property names (keys) down to children
} SchemaControl;

typedef enum {
  SC_ANY,   // "true" or empty schema (matches everything)
  SC_FALSE, // "false" schema (matches nothing)
  SC_STRING,
  SC_NUMBER,
  SC_BOOLEAN,
  SC_NULL,
  SC_OBJECT,
  SC_ARRAY,
  SC_REQUIRED_FIELD // Special type for nodes in the "required" list
} SchemaType;

typedef struct {
  TokenType type;
  struct {
    const unsigned char *start;
    size_t length;
  } name;
  union {
    struct {
      const unsigned char *start;
      size_t length;
    } string;
    double number;
  } value;
} Constraint;

typedef struct {
  SchemaType type;
  Token name; // The property name (e.g. "name")

  // --- Structure Pointers ---
  int props_head;    // Object: Linked list of Property Schemas
  int required_head; // Object: Linked list of Required Field Names
  int items_head;    // Array: Schema for items
  int next_sibling;  // Next item in the parent's list
  int first_child;
  int additional_schema; // Object: Schema for additional properties

  // --- Validation Constraints ---
  int constraints_count;
  Constraint constraints[MAX_SCHEMA_CONTRAINTS];
} SchemaNode;

int parse_schema(Stack *in, int json_idx, Stack *out);
void print_schema(Stack *out, int sc_idx, int indent);

int parse_schema_stack(Stack *json_stack, int json_root, Stack *schema_stack,
                       Stack *controls, Stack *result);

#endif
