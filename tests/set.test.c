#include "set.h"
#include "utils.h"
#include <criterion/criterion.h>
#include <string.h>

#define T set_t

/* ---------- Setup/Teardown ---------- */

static T s;

static void init() {
  /*#region*/
  test_init();
  set_init(&s);
  /*#endregion*/
}

static void fini() {
  /*#region*/
  test_fini();
  /*#endregion*/
}

/* ---------- Helpers ---------- */

/**
 * simple helper to generate unique keys like "k_0", "k_127"
 * without using sprintf/stdio.h
 */
static void int_to_key(int n, char *out) {
  /*#region*/
  char *p = out;
  *p++ = 'k';
  *p++ = '_';

  // Handle hundreds
  if (n >= 100) {
    *p++ = '0' + (n / 100);
    n %= 100;
  }
  // Handle tens
  if (n >= 10 || p > out + 2) { // check if we already wrote hundreds
    *p++ = '0' + (n / 10);
    n %= 10;
  }
  // Handle ones
  *p++ = '0' + n;
  *p = '\0';
  /*#endregion*/
}

/* ---------- Basic Functionality ---------- */

TIMED_TEST(T, empty_set_does_not_contain_items, init, fini)
/*#region*/
cr_assert_eq(set_insert(&s, "key", 3), SET_KEY_IS_UNIQ);
cr_assert_eq(set_insert(&s, "val", 3), SET_KEY_IS_UNIQ);
/*#endregion*/
END_TIMED_TEST

TIMED_TEST(T, duplicates_are_detected, init, fini)
/*#region*/
const char *key = "username";
uint16_t len = (uint16_t)strlen(key);

// 1. First insert -> (Success)
cr_assert_eq(set_insert(&s, key, len), SET_KEY_IS_UNIQ);

// 2. Second insert -> (Duplicate detected)
cr_assert_eq(set_insert(&s, key, len), SET_KEY_ALREADY_EXIST);

// 3. Third insert -> (Duplicate detected)
cr_assert_eq(set_insert(&s, key, len), SET_KEY_ALREADY_EXIST);
/*#endregion*/
END_TIMED_TEST

TIMED_TEST(T, different_keys_do_not_collide, init, fini)
/*#region*/
// "key1" and "key2" should be distinct
cr_assert_eq(set_insert(&s, "key1", 4), SET_KEY_IS_UNIQ);
cr_assert_eq(set_insert(&s, "key2", 4), SET_KEY_IS_UNIQ);

// Re-inserting key1 should be found
cr_assert_eq(set_insert(&s, "key1", 4), SET_KEY_ALREADY_EXIST);
/*#endregion*/
END_TIMED_TEST

/* ---------- Edge Cases ---------- */

TIMED_TEST(T, max_length_keys_are_handled, init, fini)
/*#region*/
char long_key[MAX_KEY_LEN];
// Fill with 'a'
for (int i = 0; i < MAX_KEY_LEN; ++i)
  long_key[i] = 'a';

// Insert max len key
cr_assert_eq(set_insert(&s, long_key, MAX_KEY_LEN), SET_KEY_IS_UNIQ);

// Check duplicate
cr_assert_eq(set_insert(&s, long_key, MAX_KEY_LEN), SET_KEY_ALREADY_EXIST);
/*#endregion*/
END_TIMED_TEST

TIMED_TEST(T, over_max_length_keys_rejected, init, fini)
/*#region*/
char too_long[MAX_KEY_LEN + 1];
// Fill with 'b'
for (size_t i = 0; i < sizeof(too_long); ++i)
  too_long[i] = 'b';

// Should return true immediately (treated as error/duplicate)
cr_assert_eq(set_insert(&s, too_long, (uint16_t)sizeof(too_long)), SET_KEY_TOO_LONG);
/*#endregion*/
END_TIMED_TEST

TIMED_TEST(T, zero_length_keys, init, fini)
/*#region*/
const char *empty = "";
// Insert empty string
cr_assert_eq(set_insert(&s, empty, 0), SET_KEY_IS_UNIQ);
// Check duplicate
cr_assert_eq(set_insert(&s, empty, 0), SET_KEY_ALREADY_EXIST);
/*#endregion*/
END_TIMED_TEST

/* ---------- Load / Capacity Tests ---------- */

TIMED_TEST(T, fill_capacity_and_check_overflow, init, fini)
/*#region*/
char buf[16];

// 1. Fill the set completely (0 to 127)
for (int i = 0; i < SET_CAPACITY; i++) {
  int_to_key(i, buf);
  int is_dup = set_insert(&s, buf, (uint16_t)strlen(buf));
  cr_expect_eq(is_dup, SET_KEY_IS_UNIQ, "Failed to insert key index %d", i);
}

// 2. Verify we can find existing items (probing works)
cr_assert_eq(set_insert(&s, "k_0", 3), SET_KEY_ALREADY_EXIST, "Should find k_0 as duplicate");
cr_assert_eq(set_insert(&s, "k_127", 5),SET_KEY_ALREADY_EXIST, "Should find k_127 as duplicate");

// 3. Verify new item fails (Table Full)
// Your code returns 'true' when probing loop finishes without empty slot
cr_assert_eq(set_insert(&s, "overflow", 8), SET_TABLE_IS_FULL,
          "Insert into full set should return true");
/*#endregion*/
END_TIMED_TEST

TIMED_TEST(T, hash_collision_probing, init, fini)
/*#region*/
// "abc" and "abd" are very likely to be close in hash space
// We insert a cluster of similar keys to force linear probing

cr_assert_eq(set_insert(&s, "abc", 3), SET_KEY_IS_UNIQ);
cr_assert_eq(set_insert(&s, "abd", 3), SET_KEY_IS_UNIQ);
cr_assert_eq(set_insert(&s, "abe", 3), SET_KEY_IS_UNIQ);
cr_assert_eq(set_insert(&s, "abf", 3), SET_KEY_IS_UNIQ);

// Ensure all are still retrievable
cr_assert_eq(set_insert(&s, "abc", 3),SET_KEY_ALREADY_EXIST);
cr_assert_eq(set_insert(&s, "abd", 3),SET_KEY_ALREADY_EXIST);
cr_assert_eq(set_insert(&s, "abe", 3),SET_KEY_ALREADY_EXIST);
cr_assert_eq(set_insert(&s, "abf", 3),SET_KEY_ALREADY_EXIST);
/*#endregion*/
END_TIMED_TEST
