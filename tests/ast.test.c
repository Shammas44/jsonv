#include "ast.h"
#include <criterion/criterion.h>

#define L Lexer
#define T AST

static Lexer *l;

static void init(void) {
  /*#region*/
  l = malloc(lexer_sizeof());
  /*#endregion*/
}

static bool run_scenario(Stack *ast, unsigned char *input) {
  /*#region*/
  lexer_init(l, input, strlen((char *)input));
  Stack control;
  unsigned char control_storage[1000] = {0};
  stack_init(&control, sizeof(int), control_storage, sizeof(control_storage));
  Stack children;
  unsigned char children_storage[1000] = {0};
  stack_init(&children, sizeof(int), children_storage,
             sizeof(children_storage));
  return jsonv_ast(l, ast, &control, &children);
  /*#endregion*/
}

static void fini(void) {
  /*#region*/
  lexer_free(&l);
  /*#endregion*/
}

Test(T, simple_valid, .init = init, .fini = fini) {
  /*#region*/
  Stack stack;
  unsigned char storage[1000] = {0};
  char *cases[] = {
      "{}",                                     //
      "{\"k1\": \"v1\", \"k2\": \"v2\"}",       //
      "{\"k1\": 1, \"k1\": 2}",                 // duplicate key
      "{\"k1\": [12,33, {\"k\": 2}] }",         //
      "{\"k1\": [12,33, {\"k\": 2}] }",         //
      "{\"k1\": [true,false, {\"k\": null}] }", //
  };
  for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
    stack_init(&stack, sizeof(ASTNode), storage, sizeof(storage));
    bool out = run_scenario(&stack, (unsigned char *)cases[i]);
    cr_expect(out);
    if (!out)
      printf("[CASE %zu]: %s\n", i, cases[i]);
    // print_ast(&stack, stack.top, 0);
  }
  /*#endregion*/
}

Test(T, simple_invalid, .init = init, .fini = fini) {
  /*#region*/
  Stack stack;
  unsigned char storage[1000] = {0};
  char *cases[] = {
      "{\"k1\": }",                       // missing value
      "{\"k1\": [true false null] }",     // missing commas
      "{\"k1\": \"v1\" \"k2\": \"v2\" }", // missing commas
      "{\"k1\": [true }",                 // unclosed bracket
      "{2:2}",                            // number as key
      "[]",                               // no object
      "null",                             // no object
      "true",                             // no object
      "false",                            // no object
      "kkk",                              // unknown type
  };
  for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
    stack_init(&stack, sizeof(ASTNode), storage, sizeof(storage));
    bool out = !run_scenario(&stack, (unsigned char *)cases[i]);
    cr_expect(out);
    if (!out)
      printf("[CASE %zu]: %s\n", i, cases[i]);
    // print_ast(&stack, stack.top, 0);
  }
  /*#endregion*/
}
