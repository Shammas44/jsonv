#include "number.h"
#include "utils.h"
#include <criterion/criterion.h>

#define T number

static void init(void) {
  /*#region*/
  test_init();
  /*#endregion*/
}

static void fini(void) {
  /*#region*/
  test_fini();
  /*#endregion*/
}

typedef struct {
  char *given;
  bool valid;
  int expected;
} IntCase;

typedef struct {
  char *given;
  bool valid;
  double expected;
} DoubleCase;

TIMED_TEST(T, parse_double, init, fini)
/*#region*/
static DoubleCase cases[] = {
    {"123.45", true, 123.45},  //
    {"123", true, 123},        //
    {"0001.45", false, 0},     // leading zero
    {" -3.14e2 ", true, -314}, //
    {" -3.14E2 ", true, -314}, //
    {"aaa42", false, 0},       //
    {"12.abc", false, 0},      //
    {"1e5000", false, 0},      // overflow
    {"", false, 0},            //
    {" ", false, 0},
};
for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
  double buff = 0;
  int e = parse_double(cases[i].given, &buff);
  char *msg = cases[i].valid ? "pass" : "fail";
  cr_assert_eq(e, cases[i].valid, "%s should have %s the test.", cases[i].given,
               msg);
  cr_expect_eq(buff, cases[i].expected, "%s should have been parsed has %f",
               cases[i].given, cases[i].expected);
}
/*#endregion*/
END_TIMED_TEST

TIMED_TEST(T, parse_int, init, fini)
/*#region*/
static IntCase cases[] = {
    {"123", true, 123},         //
    {"-55", true, -55},         //
    {"aaa42", false, 0},        //
    {"999999999999", false, 0}, // overflow
    {"", false, 0},             //
    {" ", false, 0},
};
for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
  int buff = 0;
  int e = parse_int(cases[i].given, &buff);
  cr_expect(e == cases[i].valid, "Validation is not as expected");
  cr_expect(buff == cases[i].expected, "Wrong expected value");
}
/*#endregion*/
END_TIMED_TEST
