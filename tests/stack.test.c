#include "stack.h"
#include "except.h"
#include "utils.h"
#include <criterion/criterion.h>
#include <criterion/logging.h>
#define T Stack
/* ---------- Helpers ---------- */

typedef struct {
  int id;
  double value;
} TestStruct;

static Stack s;

static void init() {
  /*#region*/
  test_init();
  /*#endregion*/
}

static void fini() {
  /*#region*/
  stack_destroy(&s);
  test_fini();
  /*#endregion*/
}

/* ---------- Initialization ---------- */

TIMED_TEST(T, empty_stack_has_top_minus_one, init, fini)
/*#region*/
int data[10] = {0};
stack_init(&s, sizeof(int), (unsigned char *)&data, sizeof(data));
cr_assert_eq(s.top, -1);
cr_assert(stack_is_empty(&s));
cr_assert_eq(stack_size(&s), 0);
/*#endregion*/
END_TIMED_TEST

/* ---------- Push ---------- */

TIMED_TEST(T, first_push_sets_top_to_zero, init, fini)
/*#region*/
int data[10] = {0};
stack_init(&s, sizeof(int), (unsigned char *)&data, sizeof(data));

int x = 42;
cr_assert(stack_push(&s, &x));
cr_assert_eq(s.top, 0);
cr_assert_eq(stack_size(&s), 1);
/*#endregion*/
END_TIMED_TEST

TIMED_TEST(T, multiple_pushes_increment_top, init, fini)
/*#region*/
int data[10] = {0};
stack_init(&s, sizeof(int), (unsigned char *)&data, sizeof(data));

for (int i = 0; i < 10; i++) {
  cr_assert(stack_push(&s, &i));
  cr_assert_eq(s.top, i);
}

cr_assert_eq(stack_size(&s), 10);
/*#endregion*/
END_TIMED_TEST

/* ---------- Pop ---------- */

TIMED_TEST(T, pop_returns_last_pushed_value, init, fini)
/*#region*/
int data[10] = {0};
stack_init(&s, sizeof(int), (unsigned char *)&data, sizeof(data));

int a = 1, b = 2;
stack_push(&s, &a);
stack_push(&s, &b);

int *out = stack_pop(&s);
cr_assert_not_null(out);
cr_assert_eq(*out, 2);
cr_assert_eq(s.top, 0);

out = stack_pop(&s);
cr_assert_not_null(out);
cr_assert_eq(*out, 1);
cr_assert_eq(s.top, -1);

cr_assert(stack_is_empty(&s));
/*#endregion*/
END_TIMED_TEST

TIMED_TEST(T, pop_on_empty_stack_fails, init, fini)
/*#region*/
int data[10] = {0};
stack_init(&s, sizeof(int), (unsigned char *)&data, sizeof(data));

TRY { stack_pop(&s); }
ELSE { cr_assert(true); }
END_TRY;
cr_assert_eq(s.top, -1);
/*#endregion*/
END_TIMED_TEST

/* ---------- Peek ---------- */

TIMED_TEST(T, peek_does_not_modify_top, init, fini)
/*#region*/
int data[10] = {0};
stack_init(&s, sizeof(int), (unsigned char *)&data, sizeof(data));

int x = 99;
stack_push(&s, &x);

int *out = stack_peek(&s, s.top);

cr_assert_not_null(out);
cr_assert_eq(*out, 99);
cr_assert_eq(s.top, 0);
/*#endregion*/
END_TIMED_TEST

/* ---------- Generic behavior ---------- */

TIMED_TEST(T, stores_and_retrieves_structs, init, fini)
/*#region*/
TestStruct data[10] = {0};
stack_init(&s, sizeof(TestStruct), (unsigned char *)&data, sizeof(data));

TestStruct a = {1, 3.14};
TestStruct b = {2, 2.71};

stack_push(&s, &a);
stack_push(&s, &b);

TestStruct *out = stack_pop(&s);
cr_assert_eq(out->id, 2);
cr_assert_float_eq(out->value, 2.71, 1e-9);

out = stack_pop(&s);
cr_assert_eq(out->id, 1);
cr_assert_float_eq(out->value, 3.14, 1e-9);
/*#endregion*/
END_TIMED_TEST

/* ---------- Growth ---------- */

TIMED_TEST(T, stack_grows_beyond_initial_capacity, init, fini)
/*#region*/
int data[10] = {0};
stack_init(&s, sizeof(int), (unsigned char *)&data, sizeof(data));

size_t count = 11;
for (size_t i = 0; i < count; i++) {
  cr_assert(stack_push(&s, &i));
}

cr_assert_eq(stack_size(&s), count);
cr_assert_eq(s.top, (int)count - 1);

for (int i = count - 1; i >= 0; i--) {
  int *out = stack_pop(&s);
  cr_assert_eq(*out, i);
}

cr_assert(stack_is_empty(&s));
/*#endregion*/
END_TIMED_TEST

/* ---------- Safety ---------- */

TIMED_TEST(T, null_arguments_fail_cleanly, init, fini)
/*#region*/
int data[10] = {0};
TRY { stack_init(&s, sizeof(int), (unsigned char *)&data, sizeof(data)); }
ELSE { cr_assert(true); }
END_TRY;

int x = 1;

TRY { stack_push(NULL, &x); }
ELSE { cr_assert(true); }
END_TRY;

TRY { stack_push(&s, NULL); }
ELSE { cr_assert(true); }
END_TRY;

TRY { stack_pop(NULL); }
ELSE { cr_assert(true); }
END_TRY;

TRY { stack_peek(NULL, x); }
ELSE { cr_assert(true); }
END_TRY;
/*#endregion*/
END_TIMED_TEST
