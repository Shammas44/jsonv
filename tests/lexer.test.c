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

TIMED_TEST(T, tokenize_with_utf8_bom, init, fini)
/*#region*/
static TokenType expected[] = {
    T_BRACE_OPEN,  // "{"
    T_STRING,      // "key1"
    T_COLON,       // ":"
    T_STRING,      // "value1"
    T_BRACE_CLOSE, // "}"
    T_EOF,
};
run_scenario("\xEF\xBB\xBF{\"key1\": \"value1\"}", S);
/*#endregion*/
END_TIMED_TEST

TIMED_TEST(T, strict_number_grammar_invalid, init, fini)
/*#region*/
static TokenType expected[] = {
    T_ERROR,
};
static char *cases[] = {
    "+10",
    "+1",
    ".5",
    "0123",
    "-0123",
    "00",
    "1.",
    "1.e2",
    "1e",
    "1e+",
    "1e-",
};
for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
  run_scenario(cases[i], S);
}
/*#endregion*/
END_TIMED_TEST

TIMED_TEST(T, strict_number_grammar_valid, init, fini)
/*#region*/
static TokenType expected[] = {
    T_NUMBER,
};
static char *cases[] = {
    "0",
    "-0",
    "123",
    "-123",
    "3.14159",
    "-0.001",
    "1e10",
    "1E-5",
    "1.23e+4",
};
for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
  run_scenario(cases[i], S);
}
/*#endregion*/
END_TIMED_TEST

TIMED_TEST(T, strict_utf8_validation, init, fini)
/*#region*/
static TokenType err_expected[] = { T_ERROR };
static char *invalid_utf8_cases[] = {
    "\"\xC0\xAF\"",         // Overlong 2-byte
    "\"\xE0\x80\xAF\"",     // Overlong 3-byte
    "\"\xED\xA0\x80\"",     // UTF-16 surrogate half U+D800 in UTF-8
    "\"\xF0\x80\x80\xAF\"", // Overlong 4-byte
    "\"\xF4\x90\x80\x80\"", // Code point > U+10FFFF
    "\"\x01\"",             // Control character < 0x20 unescaped
    "\"\x1F\"",             // Control character < 0x20 unescaped
    "\"\x80\"",             // Unexpected continuation byte
    "\"\xC2\"",             // Truncated 2-byte
    "\"\xE0\xA0\"",         // Truncated 3-byte
    "\"\xF0\x90\x80\"",     // Truncated 4-byte
};
for (size_t i = 0; i < sizeof(invalid_utf8_cases) / sizeof(invalid_utf8_cases[0]); i++) {
  run_scenario(invalid_utf8_cases[i], err_expected, 1);
}

static TokenType ok_expected[] = { T_STRING };
static char *valid_utf8_cases[] = {
    "\"hello\"",
    "\"caf\xC3\xA9\"",                     // 2-byte: café
    "\"\xE6\x97\xA5\xE6\x9C\xAC\xE8\xAA\x9E\"", // 3-byte: 日本語
    "\"\xF0\x9F\x98\x80\"",                 // 4-byte: 😀
};
for (size_t i = 0; i < sizeof(valid_utf8_cases) / sizeof(valid_utf8_cases[0]); i++) {
  run_scenario(valid_utf8_cases[i], ok_expected, 1);
}
/*#endregion*/
END_TIMED_TEST


