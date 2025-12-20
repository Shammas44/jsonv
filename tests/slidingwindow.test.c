#include "slidingwindow.h" // Assuming your code is in this header
#include <criterion/criterion.h>
#include <stdbool.h>
#define T Jsonv_SlidingWindow
T w;

// Setup function runs before every test
void setup(void) { jsonv_sw_new(&w); }

// Test 1: Ensure the window starts empty
Test(T, init_state, .init = setup) {
  /*#region*/
  cr_assert_eq(w.count, 0, "Initial count should be 0");
  cr_assert_eq(w.head, 0, "Initial head should be 0");
  /*#endregion*/
}

// Test 2: Adding elements increases count up to WINDOW_SIZE
Test(T, fill_window, .init = setup) {
  /*#region*/
  jsonv_sw_push(&w, (Token){.type = T_STRING});
  jsonv_sw_push(&w, (Token){.type = T_STRING});
  cr_assert_eq(w.count, 2, "Count should be 2 after two additions");
  /*#endregion*/
}

// Test 3: Ensure circular wrapping logic
// If WINDOW_SIZE is 8, the 9th element should overwrite the 1st
Test(T, circular_wrap, .init = setup) {
  /*#region*/
  // Fill with 'STRING'
  for (int i = 0; i < WINDOW_SIZE; i++) {
    jsonv_sw_push(&w, (Token){.type = T_STRING});
  }
  cr_assert_eq(w.count, WINDOW_SIZE);
  // Add a 'ERROR'. This should overwrite the first 'STRING'
  jsonv_sw_push(&w, (Token){.type = T_ERROR});
  cr_assert_eq(w.count, WINDOW_SIZE);
  /*#endregion*/
}

// Test 4: Verify head moves correctly
Test(T, head_movement, .init = setup) {
  /*#region*/
  jsonv_sw_push(&w, (Token){.type = T_STRING});
  jsonv_sw_push(&w, (Token){.type = T_STRING});
  cr_assert_eq(w.head, 2, "Head should point to the next insertion index");
  /*#endregion*/
}

Test(T, get_by_order, .init = setup) {
  /*#region*/
  jsonv_sw_push(&w, (Token){.type = T_BRACKET_OPEN}); // index 0
  jsonv_sw_push(&w, (Token){.type = T_COLON});        // index -1
  jsonv_sw_push(&w, (Token){.type = T_EOF});          // index -2
  Token tok1 = jsonv_sw_get_by_order(&w, 0);
  Token tok2 = jsonv_sw_get_by_order(&w, -1);
  Token tok3 = jsonv_sw_get_by_order(&w, -2);
  cr_assert_eq(w.head, 0, "Head should point to the next insertion index");
  cr_assert_eq(tok1.type, T_EOF, "Wrong tok1 type received");
  cr_assert_eq(tok2.type, T_COLON, "Wrong tok2 type received");
  cr_assert_eq(tok3.type, T_BRACKET_OPEN, "Wrong tok3 type received");
  /*#endregion*/
}
