#include "stack.h"
#include "except.h"
#include <criterion/criterion.h>
#include <criterion/logging.h>
#define T Stack
/* ---------- Helpers ---------- */

typedef struct {
  int id;
  double value;
} TestStruct;

static Stack s;

static void fini() { stack_destroy(&s); }

/* ---------- Initialization ---------- */

Test(T, empty_stack_has_top_minus_one, .fini = fini) {
  int data[10] = {0};
  stack_init(&s, sizeof(int), (unsigned char *)&data, sizeof(data));
  cr_assert_eq(s.top, -1);
  cr_assert(stack_is_empty(&s));
  cr_assert_eq(stack_size(&s), 0);
}

/* ---------- Push ---------- */

Test(T, first_push_sets_top_to_zero, .fini = fini) {
  int data[10] = {0};
  Stack s;
  stack_init(&s, sizeof(int), (unsigned char *)&data, sizeof(data));

  int x = 42;
  cr_assert(stack_push(&s, &x));
  cr_assert_eq(s.top, 0);
  cr_assert_eq(stack_size(&s), 1);
}

Test(T, multiple_pushes_increment_top, .fini = fini) {
  int data[10] = {0};
  Stack s;
  stack_init(&s, sizeof(int), (unsigned char *)&data, sizeof(data));

  for (int i = 0; i < 10; i++) {
    cr_assert(stack_push(&s, &i));
    cr_assert_eq(s.top, i);
  }

  cr_assert_eq(stack_size(&s), 10);
}

/* ---------- Pop ---------- */

Test(T, pop_returns_last_pushed_value, .fini = fini) {
  int data[10] = {0};
  Stack s;
  stack_init(&s, sizeof(int), (unsigned char *)&data, sizeof(data));

  int a = 1, b = 2;
  stack_push(&s, &a);
  stack_push(&s, &b);

  int out = 0;
  cr_assert(stack_pop(&s, &out));
  cr_assert_eq(out, 2);
  cr_assert_eq(s.top, 0);

  cr_assert(stack_pop(&s, &out));
  cr_assert_eq(out, 1);
  cr_assert_eq(s.top, -1);

  cr_assert(stack_is_empty(&s));
}

Test(T, pop_on_empty_stack_fails, .fini = fini) {
  int data[10] = {0};
  Stack s;
  stack_init(&s, sizeof(int), (unsigned char *)&data, sizeof(data));

  int out;
  cr_assert_not(stack_pop(&s, &out));
  cr_assert_eq(s.top, -1);
}

/* ---------- Peek ---------- */

Test(T, peek_does_not_modify_top, .fini = fini) {
  int data[10] = {0};
  Stack s;
  stack_init(&s, sizeof(int), (unsigned char *)&data, sizeof(data));

  int x = 99;
  stack_push(&s, &x);

  int out = 0;
  cr_assert(stack_peek(&s, &out));
  cr_assert_eq(out, 99);
  cr_assert_eq(s.top, 0);
}

/* ---------- Generic behavior ---------- */

Test(T, stores_and_retrieves_structs, .fini = fini) {
  TestStruct data[10] = {0};
  Stack s;
  stack_init(&s, sizeof(TestStruct), (unsigned char *)&data, sizeof(data));

  TestStruct a = {1, 3.14};
  TestStruct b = {2, 2.71};

  stack_push(&s, &a);
  stack_push(&s, &b);

  TestStruct out;
  stack_pop(&s, &out);
  cr_assert_eq(out.id, 2);
  cr_assert_float_eq(out.value, 2.71, 1e-9);

  stack_pop(&s, &out);
  cr_assert_eq(out.id, 1);
  cr_assert_float_eq(out.value, 3.14, 1e-9);
}

/* ---------- Growth ---------- */

Test(T, stack_grows_beyond_initial_capacity, .fini = fini) {
  int data[10] = {0};
  Stack s;
  stack_init(&s, sizeof(int), (unsigned char *)&data, sizeof(data));

  size_t count = 11;
  for (size_t i = 0; i < count; i++) {
    cr_assert(stack_push(&s, &i));
  }

  cr_assert_eq(stack_size(&s), count);
  cr_assert_eq(s.top, (int)count - 1);

  int out;
  for (int i = count - 1; i >= 0; i--) {
    stack_pop(&s, &out);
    cr_assert_eq(out, i);
  }

  cr_assert(stack_is_empty(&s));
}

/* ---------- Safety ---------- */

Test(T, null_arguments_fail_cleanly, .fini = fini) {
  int data[10] = {0};
  Stack s;
  TRY { stack_init(&s, sizeof(int), (unsigned char *)&data, sizeof(data)); }
  ELSE {
    // expect to be called
    cr_assert(true);
  }
  END_TRY;

  int x = 1;
  cr_assert_not(stack_push(NULL, &x));
  cr_assert_not(stack_push(&s, NULL));

  cr_assert_not(stack_pop(NULL, &x));
  cr_assert_not(stack_peek(NULL, &x));
}
