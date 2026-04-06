#include "lexer.h"
#include "utils.h"
#include <criterion/criterion.h>

#define T Lexer
#define S expected, sizeof(expected) / sizeof(expected[0])

static Lexer *l;

static void init(void) {
  /*#region*/
  test_init();
  l = malloc(lexer_sizeof());
  /*#endregion*/
}

static void fini(void) {
  /*#region*/
  lexer_free(&l);
  test_fini();
  /*#endregion*/
}

static void run_scenario(char *input, TokenType expected[], int length) {
  /*#region*/
  lexer_init(l, (unsigned char *)input, strlen((char *)input));
  for (int i = 0; i < length; i++) {
    Token t = lexer_next_token(l);
    if (t.type != expected[i])
      printf("[%d] expected: %d, received: %d\n", i, expected[i], t.type);
    cr_assert_eq(t.type, expected[i]);
  }
  /*#endregion*/
}

TIMED_TEST(T, wrong_start_token, init, fini)
/*#region*/
static TokenType expected[] = {
    T_ERROR,
};
static char *cases[] = {
    "t", "f", "nul", "\"key", ".",
};
for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
  run_scenario(cases[i], S);
}
/*#endregion*/
END_TIMED_TEST

TIMED_TEST(T, tokenize_simple, init, fini)
/*#region*/
static TokenType expected[] = {
    T_BRACE_OPEN,  // "{"
    T_STRING,      // "key1"
    T_COLON,       // ":"
    T_STRING,      // "value1"
    T_COMMA,       // ","
    T_STRING,      // "key2"
    T_COLON,       // ":"
    T_STRING,      // "value2"
    T_BRACE_CLOSE, // "}"
    T_EOF,         //
};
run_scenario("{\"key1\": \"value1\", \"key2\": \"value2\" }", S);
/*#endregion*/
END_TIMED_TEST
