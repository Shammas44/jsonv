#include "number.h"
#include <criterion/criterion.h>

#define T number

typedef struct {
  char *given;
  bool valid;
  int expected;
} Case;

Test(T, parse_int) {
  /*#region*/
  static Case cases[] = {
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
}
