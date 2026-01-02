// #include "lexer.h"
// #include <criterion/criterion.h>

// #define T Lexer
// #define S expected, sizeof(expected) / sizeof(expected[0])

// static Lexer *l;

// static void init(void) {
//   /*#region*/
//   l = malloc(lexer_sizeof());
//   /*#endregion*/
// }

// static void run_scenario(char *input, TokenType expected[], int length) {
//   /*#region*/
//   lexer_init(&l, input, strlen(input));
//   for (int i = 0; i < length; i++) {
//     Token t = lexer_next_token(l);
//     if (t.type != expected[i])
//       printf("[%d] expected: %d, received: %d\n", i, expected[i], t.type);
//     cr_assert_eq(t.type, expected[i]);
//   }
//   /*#endregion*/
// }

// static void fini(void) {
//   /*#region*/
//   lexer_free(&l);
//   /*#endregion*/
// }

// Test(T, tokenize_simple, .init = init, .fini = fini) {
//   /*#region*/
//   static TokenType expected[] = {
//       T_BRACE_OPEN,  // "{"
//       T_STRING,      // "key1"
//       T_COLON,       // ":"
//       T_STRING,      // "value1"
//       T_COMMA,       // ","
//       T_STRING,      // "key2"
//       T_COLON,       // ":"
//       T_STRING,      // "value2"
//       T_BRACE_CLOSE, // "}"
//       T_EOF,         //
//   };
//   run_scenario("{\"key1\": \"value1\", \"key2\": \"value2\" }", S);
//   /*#endregion*/
// }

// Test(T, wrong_start_token, .init = init, .fini = fini) {
//   /*#region*/
//   static TokenType expected[] = {
//       T_ERROR,
//   };
//   static char *cases[] = {
//       "]", "[", "}", "\"key\"", ":", ",", "12", "true", "false", "null",
//   };
//   for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
//     run_scenario(cases[i], S);
//   }
//   /*#endregion*/
// }

// Test(T, empty, .init = init, .fini = fini) {
//   /*#region*/
//   static TokenType expected[] = {
//       T_BRACE_OPEN,
//       T_BRACE_CLOSE,
//   };
//   run_scenario("{}", S);
//   /*#endregion*/
// }

// Test(T, brace_cannot_be_followed_by_brace, .init = init, .fini = fini) {
//   /*#region*/
//   static TokenType expected[] = {
//       T_BRACE_OPEN,
//       T_ERROR,
//   };
//   run_scenario("{{}}", S);
//   /*#endregion*/
// }
