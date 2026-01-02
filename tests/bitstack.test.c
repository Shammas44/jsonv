#include "bitstack.h" // Assuming your code is in this header
#include <criterion/criterion.h>
#include <stdbool.h>
#define T Jsonv_BitStack
static T *w;

static void setup(void) {
  /*#region*/
  w = malloc(jsonv_bs_sizeof());
  jsonv_bs_init(w);
  /*#endregion*/
}

static void fini(void) {
  /*#region*/
  free(w);
  /*#endregion*/
}

// Test 1: Ensure the window starts empty
Test(T, init_state, .init = setup, .fini = fini) {
  /*#region*/
  cr_assert_eq(jsonv_bs_top(w), -1);
  cr_assert_eq(jsonv_bs_is_empty(w), true);
  cr_assert_eq(jsonv_bs_is_full(w), false);
  /*#endregion*/
}

Test(T, push_and_pop, .init = setup, .fini = fini) {
  /*#region*/
  bool input[] = {true, false, true, false, true, false};
  for (size_t i = 0; i < sizeof(input) / sizeof(input[0]); i++) {
    jsonv_bs_push(w, input[i]);
    bool value = jsonv_bs_top(w);
    cr_assert_eq(input[i], value);
  }
  for (int i = sizeof(input) / sizeof(input[0]) - 1; i >= 0; i--) {
    bool value = jsonv_bs_pop(w);
    cr_assert_eq(input[i], value);
  }
  /*#endregion*/
}
