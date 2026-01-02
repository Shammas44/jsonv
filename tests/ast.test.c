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

static bool run_scenario(char *input) {
  /*#region*/
  lexer_init(&l, input, strlen(input));
  return validate_json(l);
  /*#endregion*/
}

static void fini(void) {
  /*#region*/
  lexer_free(&l);
  /*#endregion*/
}

// Test(T, tokenize_simple, .init = init, .fini = fini) {
//   /*#region*/
//   bool value = run_scenario("{\"key1\": \"value1\", \"key2\": \"value2\" }");
//   cr_assert_eq(value, true);
//   /*#endregion*/
// }

Test(T, case2, .init = init, .fini = fini) {
  /*#region*/
  bool value = run_scenario("{\"k1\": [12,33, {\"k\": 2}] }");
  cr_assert_eq(value, true);
  /*#endregion*/
}
