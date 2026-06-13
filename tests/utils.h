#ifndef _JSONV_TEST_UTILS_H
#define _JSONV_TEST_UTILS_H
#include <criterion/criterion.h>
#include <stdint.h>

/* ---------- per-test state ---------- */

extern const char *current_test_name;

/* ---------- helper macro ---------- */
void test_init(void);

void test_fini(void);

#define STR_HELPER(x) #x
#define STR(x) STR_HELPER(x)
/*
 * Must be called at the beginning of each test body
 */
#define WARN_THRESHOLD_MS 50

#define SET_TEST_NAME(suite, test) current_test_name = STR(suite) ":" STR(test)

#define TIMED_TEST(suite, name, f_init, f_fini)                                \
  Test(suite, name, .init = f_init, .fini = f_fini) {                          \
    SET_TEST_NAME(suite, name);

#define END_TIMED_TEST }
#endif
