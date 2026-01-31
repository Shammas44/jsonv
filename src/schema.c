#include "schema.h"
#include <memd/memd.h>
#include "assert.h"
#include "ast.h"
#include <stdio.h>
#include <string.h>

// --- UTILS ---

static double get_json_double(ASTNode *json_pool, int node_idx) {
  /*#region*/
  if (node_idx == -1)
    return 0.0;
  return json_pool[node_idx].token.value.number;
  /*#endregion*/
}

static Token get_json_token(ASTNode *json_pool, int node_idx) {
  /*#region*/
  if (node_idx == -1)
    return (Token){0};
  return json_pool[node_idx].token;
  /*#endregion*/
}

// Wrapper for AST property finder
// Assumes jsonv_find_property handles quote unwrapping internally
extern int jsonv_find_property(ASTNode *pool, int idx, const char *key);

// --- CONSTRAINT LOGIC ---

typedef void (*ConstraintSetter)(SchemaNode *node, int idx, ASTNode *json_pool,
                                 int val_idx);

static void set_number_constraint(SchemaNode *n, int i, ASTNode *pool,
                                  int idx) {
  /*#region*/
  n->constraints[i].type = T_NUMBER;
  n->constraints[i].value.number = get_json_double(pool, idx);
  /*#endregion*/
}

static void set_string_constraint(SchemaNode *n, int i, ASTNode *pool,
                                  int idx) {
  /*#region*/
  n->constraints[i].type = T_STRING;
  Token t = get_json_token(pool, idx);
  n->constraints[i].value.string.start = t.value.string.start;
  n->constraints[i].value.string.length = t.value.string.length;
  /*#endregion*/
}

static void set_boolean_constraint(SchemaNode *n, int i, ASTNode *pool,
                                   int idx) {
  /*#region*/
  n->constraints[i].type = pool[idx].token.type;
  /*#endregion*/
}

typedef struct {
  const char *key;
  ConstraintSetter setter;
} ConstraintDef;

static ConstraintDef kConstraints[] = {
    // Numeric
    {"minLength", set_number_constraint},
    {"maxLength", set_number_constraint},
    {"minimum", set_number_constraint},
    {"maximum", set_number_constraint},
    {"exclusiveMinimum", set_number_constraint},
    {"exclusiveMaximum", set_number_constraint},
    {"multipleOf", set_number_constraint},
    {"minItems", set_number_constraint},
    {"maxItems", set_number_constraint},
    {"minProperties", set_number_constraint},
    {"maxProperties", set_number_constraint},

    // String
    {"pattern", set_string_constraint},
    {"format", set_string_constraint},

    // Boolean (Centralized!)
    {"additionalProperties", set_boolean_constraint},
    {"uniqueItems", set_boolean_constraint},
};
#define NUM_CONSTRAINTS (sizeof(kConstraints) / sizeof(kConstraints[0]))

static void extract_constraints(ASTNode *json_pool, int json_idx,
                                SchemaNode *schema) {
  /*#region*/
  for (size_t i = 0; i < NUM_CONSTRAINTS; i++) {
    int val_idx = jsonv_find_property(json_pool, json_idx, kConstraints[i].key);

    if (val_idx != -1 && schema->constraints_count < MAX_SCHEMA_CONTRAINTS) {
      int c_idx = schema->constraints_count++;

      size_t key_len = strlen(kConstraints[i].key);
      schema->constraints[c_idx].name.start =
          (const unsigned char *)kConstraints[i].key;
      schema->constraints[c_idx].name.length = key_len;

      kConstraints[i].setter(schema, c_idx, json_pool, val_idx);
    }
  }
  /*#endregion*/
}

// --- PARSER HELPERS ---

static int new_schema_node(Stack *out, SchemaType type) {
  /*#region*/
  SchemaNode node;
  memset(&node, 0, sizeof(SchemaNode));
  node.type = type;
  node.first_child = -1;
  node.props_head = -1;
  node.required_head = -1;
  node.items_head = -1;
  node.next_sibling = -1;
  node.additional_schema_head = -1;
  node.pattern_props_head = -1;
  stack_push(out, &node);
  return out->top;
  /*#endregion*/
}

static SchemaType parse_type_string(Token t) {
  /*#region*/
  if (strncmp((char *)t.value.string.start, "object", t.value.string.length) == 0)
    return SC_OBJECT;
  if (strncmp((char *)t.value.string.start, "array", t.value.string.length) == 0)
    return SC_ARRAY;
  if (strncmp((char *)t.value.string.start, "string", t.value.string.length) == 0)
    return SC_STRING;
  if (strncmp((char *)t.value.string.start, "number", t.value.string.length) == 0)
    return SC_NUMBER;
  if (strncmp((char *)t.value.string.start, "integer", t.value.string.length) == 0)
    return SC_NUMBER;
  if (strncmp((char *)t.value.string.start, "boolean", t.value.string.length) == 0)
    return SC_BOOLEAN;
  if (strncmp((char *)t.value.string.start, "null", t.value.string.length) == 0)
    return SC_NULL;
  return SC_ANY;
  /*#endregion*/
}

static SchemaType determine_type(ASTNode *json_pool, int json_idx) {
  /*#region*/
  ASTNode *node = &json_pool[json_idx];

  // Handle Boolean Schemas (e.g. "additionalProperties": false)
  if (node->type == AST_LEAF) {
    if (node->token.type == T_FALSE)
      return SC_FALSE;
    return SC_ANY;
  }

  // Explicit type definition
  int type_idx = jsonv_find_property(json_pool, json_idx, "type");
  if (type_idx != -1) {
    return parse_type_string(json_pool[type_idx].token);
  }

  // Inference
  if (jsonv_find_property(json_pool, json_idx, "properties") != -1)
    return SC_OBJECT;
  if (jsonv_find_property(json_pool, json_idx, "items") != -1)
    return SC_ARRAY;

  return SC_ANY;
  /*#endregion*/
}

// --- STACK SCHEDULERS ---

static void schedule_properties(Stack *controls, ASTNode *json_pool,
                                int json_props_idx, int schema_idx) {
  /*#region*/
  if (json_props_idx == -1 || json_pool[json_props_idx].type != AST_OBJECT)
    return;

  int count = 0;
  int curr = json_pool[json_props_idx].first_child;
  while (curr != -1) {
    count++;
    int val = json_pool[curr].next_sibling;
    curr = json_pool[val].next_sibling;
  }

  SchemaControl conn = {
      .cmd = SC_CMD_CONNECT_PROPS, .schema_idx = schema_idx, .count = count};
  stack_push(controls, &conn);

  curr = json_pool[json_props_idx].first_child;
  while (curr != -1) {
    int val_idx = json_pool[curr].next_sibling;
    SchemaControl task = {.cmd = SC_CMD_BUILD_NODE,
                          .json_idx = val_idx,
                          .name_override = json_pool[curr].token};
    stack_push(controls, &task);
    curr = json_pool[val_idx].next_sibling;
  }
  /*#endregion*/
}

static void schedule_pattern_properties(Stack *controls, ASTNode *json_pool,
                                        int json_props_idx, int schema_idx) {
  /*#region*/
  if (json_props_idx == -1 || json_pool[json_props_idx].type != AST_OBJECT)
    return;

  int count = 0;
  int curr = json_pool[json_props_idx].first_child;
  while (curr != -1) {
    count++;
    int val = json_pool[curr].next_sibling;
    curr = json_pool[val].next_sibling;
  }

  SchemaControl conn = {.cmd = SC_CMD_CONNECT_PATTERN_PROPS,
                        .schema_idx = schema_idx,
                        .count = count};
  stack_push(controls, &conn);

  curr = json_pool[json_props_idx].first_child;
  while (curr != -1) {
    int val_idx = json_pool[curr].next_sibling;
    SchemaControl task = {.cmd = SC_CMD_BUILD_NODE,
                          .json_idx = val_idx,
                          .name_override = json_pool[curr].token};
    stack_push(controls, &task);
    curr = json_pool[val_idx].next_sibling;
  }
  /*#endregion*/
}

static void schedule_required(Stack *controls, ASTNode *json_pool,
                              int json_req_idx, int schema_idx) {
  /*#region*/

  if (json_req_idx == -1 || json_pool[json_req_idx].type != AST_ARRAY)
    return;

  int count = 0;
  int curr = json_pool[json_req_idx].first_child;
  while (curr != -1) {
    count++;
    curr = json_pool[curr].next_sibling;
  }

  SchemaControl conn = {
      .cmd = SC_CMD_CONNECT_REQUIRED, .schema_idx = schema_idx, .count = count};
  stack_push(controls, &conn);

  curr = json_pool[json_req_idx].first_child;
  while (curr != -1) {
    SchemaControl task = {.cmd = SC_CMD_BUILD_REQUIRED_NODE, .json_idx = curr};
    stack_push(controls, &task);
    curr = json_pool[curr].next_sibling;
  }
  /*#endregion*/
}

// --- CORE PARSER ---

int parse_schema_stack(Stack *json, int json_root, Stack *schema,
                       Stack *controls, Stack *results) {
  /*#region*/
  SchemaControl start = {.cmd = SC_CMD_BUILD_NODE, .json_idx = json_root};
  stack_push(controls, &start);

  while (controls->top >= 0) {
    SchemaControl frame = *(SchemaControl *)stack_pop(controls);
    ASTNode *json_pool = (ASTNode *)json->data;

    switch (frame.cmd) {

    case SC_CMD_BUILD_NODE: {
      if (frame.json_idx == -1)
        break;

      // 1. Create Node
      SchemaType type = determine_type(json_pool, frame.json_idx);
      int sc_idx = new_schema_node(schema, type);
      SCH_NODE_FROM_STACK(schema, sc_idx)->name = frame.name_override;

      // 2. Extract Centralized Constraints
      extract_constraints(json_pool, frame.json_idx,
                          SCH_NODE_FROM_STACK(schema, sc_idx));

      // 3. Schedule Return (Executes LAST)
      SchemaControl ret = {.cmd = SC_CMD_RETURN, .schema_idx = sc_idx};
      stack_push(controls, &ret);

      // 4. Schedule Structural Children (Executes FIRST)
      if (type == SC_OBJECT) {
        // Properties
        int props_idx =
            jsonv_find_property(json_pool, frame.json_idx, "properties");
        schedule_properties(controls, json_pool, props_idx, sc_idx);

        // patternProperties
        int pat_props_idx =
            jsonv_find_property(json_pool, frame.json_idx, "patternProperties");
        schedule_pattern_properties(controls, json_pool, pat_props_idx, sc_idx);

        // Required
        int req_idx =
            jsonv_find_property(json_pool, frame.json_idx, "required");
        schedule_required(controls, json_pool, req_idx, sc_idx);

        // Additional Properties (as a Schema)
        int add_idx = jsonv_find_property(json_pool, frame.json_idx,
                                          "additionalProperties");
        if (add_idx != -1) {
          SchemaControl conn = {.cmd = SC_CMD_CONNECT_ADDITIONAL_PROPS,
                                .schema_idx = sc_idx};
          stack_push(controls, &conn);
          SchemaControl build = {.cmd = SC_CMD_BUILD_NODE, .json_idx = add_idx};
          stack_push(controls, &build);
        }
      } else if (type == SC_ARRAY) {
        // Items
        int items_idx = jsonv_find_property(json_pool, frame.json_idx, "items");
        if (items_idx != -1) {
          SchemaControl conn = {.cmd = SC_CMD_CONNECT_ITEMS,
                                .schema_idx = sc_idx};
          stack_push(controls, &conn);
          SchemaControl build = {.cmd = SC_CMD_BUILD_NODE,
                                 .json_idx = items_idx};
          stack_push(controls, &build);
        }
      }
      break;
    }

    case SC_CMD_BUILD_REQUIRED_NODE: {
      int idx = new_schema_node(schema, SC_REQUIRED_FIELD);
      SCH_NODE_FROM_STACK(schema, idx)->name = json_pool[frame.json_idx].token;
      stack_push(results, &idx);
      break;
    }

      // --- LINKING COMMANDS ---

    case SC_CMD_CONNECT_PROPS: {
      int head = -1, prev = -1;
      for (int i = 0; i < frame.count; i++) {
        int child = *(int *)stack_pop(results);
        if (head == -1)
          head = child;
        else
          SCH_NODE_FROM_STACK(schema, prev)->next_sibling = child;
        prev = child;
      }
      SCH_NODE_FROM_STACK(schema, frame.schema_idx)->props_head = head;
      break;
    }

    case SC_CMD_CONNECT_PATTERN_PROPS: {
      int head = -1, prev = -1;
      for (int i = 0; i < frame.count; i++) {
        int child = *(int *)stack_pop(results);
        if (head == -1)
          head = child;
        else
          SCH_NODE_FROM_STACK(schema, prev)->next_sibling = child;
        prev = child;
      }
      SCH_NODE_FROM_STACK(schema, frame.schema_idx)->pattern_props_head = head;
      break;
    }

    case SC_CMD_CONNECT_REQUIRED: {
      int head = -1, prev = -1;
      for (int i = 0; i < frame.count; i++) {
        int child = *(int *)stack_pop(results);
        if (head == -1)
          head = child;
        else
          SCH_NODE_FROM_STACK(schema, prev)->next_sibling = child;
        prev = child;
      }
      SCH_NODE_FROM_STACK(schema, frame.schema_idx)->required_head = head;
      break;
    }

    case SC_CMD_CONNECT_ITEMS: {
      int child = *(int *)stack_pop(results);
      SCH_NODE_FROM_STACK(schema, frame.schema_idx)->items_head = child;
      break;
    }

    case SC_CMD_CONNECT_ADDITIONAL_PROPS: {
      int child = *(int *)stack_pop(results);
      SCH_NODE_FROM_STACK(schema, frame.schema_idx)->additional_schema_head =
          child;
      break;
    }

    case SC_CMD_RETURN: {
      stack_push(results, &frame.schema_idx);
      break;
    }
    }
  }

  return (results->top >= 0) ? *(int *)stack_pop(results) : -1;
  /*#endregion*/
}

// --- PRINTER IMPLEMENTATION ---

void print_schema_internal(Stack *out, int sc_idx, int indent, bool traverse_siblings) {
  /*#region*/
  if (sc_idx == -1)
    return;
    
  SchemaNode *node = SCH_NODE_FROM_STACK(out, sc_idx);

  // 1. Print Indentation
  for (int i = 0; i < indent; i++)
    printf("  ");

  // 2. Print Name/Type
  if (node->name.value.string.length > 0)
    printf("Property '%.*s': ", (int)node->name.value.string.length,
           node->name.value.string.start);
  else
    printf("Schema: ");

  const char *type_names[] = {"ANY",  "FALSE",  "STRING", "NUMBER", "BOOL",
                              "NULL", "OBJECT", "ARRAY",  "REQ"};
  
  if (node->type >= 0 && node->type < 9)
      printf("%s", type_names[node->type]);
  else
      printf("UNKNOWN(%d)", node->type);

  // 3. Print Constraints
  for (int i = 0; i < node->constraints_count; i++) {
    Constraint *c = &node->constraints[i];
    printf(" [%.*s: ", (int)c->name.length, c->name.start);
    
    if (c->type == T_NUMBER)
      printf("%.2f]", c->value.number);
    else if (c->type == T_STRING)
      printf("\"%.*s\"]", (int)c->value.string.length, c->value.string.start);
    else if (c->type == T_TRUE)
      printf("true]");
    else if (c->type == T_FALSE)
      printf("false]");
  }
  printf("\n");

  // 4. Handle Object Children
  if (node->type == SC_OBJECT) {
    // Print Required Fields
    if (node->required_head != -1) {
      for (int i = 0; i < indent + 1; i++)
        printf("  ");
      printf("[Required: ");
      int curr = node->required_head;
      while (curr != -1) {
        Token t = SCH_NODE_FROM_STACK(out, curr)->name;
        printf("%.*s ", (int)t.value.string.length, t.value.string.start);
        curr = SCH_NODE_FROM_STACK(out, curr)->next_sibling;
      }
      printf("]\n");
    }

    // Recurse on Properties
    print_schema_internal(out, node->props_head, indent + 1, true);

    // NEW: Recurse on Pattern Properties
    if (node->pattern_props_head != -1) {
        for (int i = 0; i < indent + 1; i++) printf("  ");
        printf("PatternProperties:\n");
        // We traverse siblings because patternProperties is a map (list of nodes)
        print_schema_internal(out, node->pattern_props_head, indent + 2, true);
    }

    // Recurse on Additional Properties
    if (node->additional_schema_head != -1) {
      for (int i = 0; i < indent + 1; i++)
        printf("  ");
      printf("AdditionalProperties:\n");
      
      // Pass 'false' to avoid printing siblings for additionalSchema
      print_schema_internal(out, node->additional_schema_head, indent + 2, false);
    }
  } 
  // 5. Handle Array Children
  else if (node->type == SC_ARRAY) {
    print_schema_internal(out, node->items_head, indent + 1, true);
  }

  // 6. Handle Siblings (Recursion)
  if (traverse_siblings && node->next_sibling != -1) {
    print_schema_internal(out, node->next_sibling, indent, true);
  }
  /*#endregion*/
}

void print_schema(Stack *out, int sc_idx, int indent) {
    /*#region*/
    print_schema_internal(out, sc_idx, indent, true);
    /*#endregion*/
}
