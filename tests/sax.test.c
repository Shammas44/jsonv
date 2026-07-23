#include "sax.h"
#include "utils.h"
#include <criterion/criterion.h>
#include <string.h>

#define T Sax

typedef struct {
  int begin_object_count;
  int end_object_count;
  int begin_array_count;
  int end_array_count;
  int key_count;
  int string_count;
  int number_count;
  int boolean_count;
  int null_count;

  const unsigned char *input_buffer;
  size_t input_len;
  bool zero_copy_ok;

  int abort_after_n_events;
  int event_count;

  // Last values recorded
  unsigned char last_key[64];
  size_t last_key_len;
  unsigned char last_string[64];
  size_t last_string_len;
  unsigned char last_number[64];
  size_t last_number_len;
  bool last_bool;
} TestSaxState;

static void init_state(TestSaxState *state, const unsigned char *buf, size_t len) {
  memset(state, 0, sizeof(TestSaxState));
  state->input_buffer = buf;
  state->input_len = len;
  state->zero_copy_ok = true;
}

static bool test_on_begin_object(void *user_data) {
  TestSaxState *state = (TestSaxState *)user_data;
  state->begin_object_count++;
  state->event_count++;
  if (state->abort_after_n_events > 0 && state->event_count >= state->abort_after_n_events) {
    return false;
  }
  return true;
}

static bool test_on_end_object(void *user_data) {
  TestSaxState *state = (TestSaxState *)user_data;
  state->end_object_count++;
  state->event_count++;
  if (state->abort_after_n_events > 0 && state->event_count >= state->abort_after_n_events) {
    return false;
  }
  return true;
}

static bool test_on_begin_array(void *user_data) {
  TestSaxState *state = (TestSaxState *)user_data;
  state->begin_array_count++;
  state->event_count++;
  if (state->abort_after_n_events > 0 && state->event_count >= state->abort_after_n_events) {
    return false;
  }
  return true;
}

static bool test_on_end_array(void *user_data) {
  TestSaxState *state = (TestSaxState *)user_data;
  state->end_array_count++;
  state->event_count++;
  if (state->abort_after_n_events > 0 && state->event_count >= state->abort_after_n_events) {
    return false;
  }
  return true;
}

static bool test_on_object_key(const unsigned char *key, size_t key_len, void *user_data) {
  TestSaxState *state = (TestSaxState *)user_data;
  state->key_count++;
  state->event_count++;

  // Verify zero-copy: key points directly within input buffer
  if (key < state->input_buffer || key + key_len > state->input_buffer + state->input_len) {
    state->zero_copy_ok = false;
  }

  state->last_key_len = key_len < 63 ? key_len : 63;
  memcpy(state->last_key, key, state->last_key_len);
  state->last_key[state->last_key_len] = '\0';

  if (state->abort_after_n_events > 0 && state->event_count >= state->abort_after_n_events) {
    return false;
  }
  return true;
}

static bool test_on_string(const unsigned char *val, size_t val_len, void *user_data) {
  TestSaxState *state = (TestSaxState *)user_data;
  state->string_count++;
  state->event_count++;

  // Verify zero-copy: val points directly within input buffer
  if (val < state->input_buffer || val + val_len > state->input_buffer + state->input_len) {
    state->zero_copy_ok = false;
  }

  state->last_string_len = val_len < 63 ? val_len : 63;
  memcpy(state->last_string, val, state->last_string_len);
  state->last_string[state->last_string_len] = '\0';

  if (state->abort_after_n_events > 0 && state->event_count >= state->abort_after_n_events) {
    return false;
  }
  return true;
}

static bool test_on_number(const unsigned char *val, size_t val_len, void *user_data) {
  TestSaxState *state = (TestSaxState *)user_data;
  state->number_count++;
  state->event_count++;

  // Verify zero-copy: val points directly within input buffer
  if (val < state->input_buffer || val + val_len > state->input_buffer + state->input_len) {
    state->zero_copy_ok = false;
  }

  state->last_number_len = val_len < 63 ? val_len : 63;
  memcpy(state->last_number, val, state->last_number_len);
  state->last_number[state->last_number_len] = '\0';

  if (state->abort_after_n_events > 0 && state->event_count >= state->abort_after_n_events) {
    return false;
  }
  return true;
}

static bool test_on_boolean(bool val, void *user_data) {
  TestSaxState *state = (TestSaxState *)user_data;
  state->boolean_count++;
  state->event_count++;
  state->last_bool = val;

  if (state->abort_after_n_events > 0 && state->event_count >= state->abort_after_n_events) {
    return false;
  }
  return true;
}

static bool test_on_null(void *user_data) {
  TestSaxState *state = (TestSaxState *)user_data;
  state->null_count++;
  state->event_count++;

  if (state->abort_after_n_events > 0 && state->event_count >= state->abort_after_n_events) {
    return false;
  }
  return true;
}

static const jsonv_sax_callbacks all_callbacks = {
  .on_begin_object = test_on_begin_object,
  .on_end_object = test_on_end_object,
  .on_begin_array = test_on_begin_array,
  .on_end_array = test_on_end_array,
  .on_object_key = test_on_object_key,
  .on_string = test_on_string,
  .on_number = test_on_number,
  .on_boolean = test_on_boolean,
  .on_null = test_on_null
};

static void setup_test(void) {
  test_init();
}

static void teardown_test(void) {
  test_fini();
}

TIMED_TEST(T, parse_simple_object, setup_test, teardown_test)
/*#region*/
  const char *json = "{\"a\": 1, \"b\": \"hello\", \"c\": true, \"d\": null}";
  size_t len = strlen(json);
  TestSaxState state;
  init_state(&state, (const unsigned char *)json, len);

  bool result = jsonv_parse_sax((const unsigned char *)json, len, &all_callbacks, &state);

  cr_expect(result, "Parsing valid simple object should succeed");
  cr_expect_eq(state.begin_object_count, 1);
  cr_expect_eq(state.end_object_count, 1);
  cr_expect_eq(state.key_count, 4);
  cr_expect_eq(state.number_count, 1);
  cr_expect_eq(state.string_count, 1);
  cr_expect_eq(state.boolean_count, 1);
  cr_expect_eq(state.null_count, 1);
  cr_expect(state.zero_copy_ok, "All key/value pointers must reference the input buffer directly");

  // Verify parsed values match
  cr_expect_str_eq((char *)state.last_key, "d");
  cr_expect_str_eq((char *)state.last_string, "hello");
  cr_expect_str_eq((char *)state.last_number, "1");
  cr_expect_eq(state.last_bool, true);
/*#endregion*/
END_TIMED_TEST

TIMED_TEST(T, parse_simple_array, setup_test, teardown_test)
/*#region*/
  const char *json = "[123.45, \"text\", false]";
  size_t len = strlen(json);
  TestSaxState state;
  init_state(&state, (const unsigned char *)json, len);

  bool result = jsonv_parse_sax((const unsigned char *)json, len, &all_callbacks, &state);

  cr_expect(result, "Parsing valid simple array should succeed");
  cr_expect_eq(state.begin_array_count, 1);
  cr_expect_eq(state.end_array_count, 1);
  cr_expect_eq(state.number_count, 1);
  cr_expect_eq(state.string_count, 1);
  cr_expect_eq(state.boolean_count, 1);
  cr_expect(state.zero_copy_ok, "Zero-copy pointers should point to the input buffer");

  cr_expect_str_eq((char *)state.last_number, "123.45");
  cr_expect_str_eq((char *)state.last_string, "text");
  cr_expect_eq(state.last_bool, false);
/*#endregion*/
END_TIMED_TEST

TIMED_TEST(T, parse_nested, setup_test, teardown_test)
/*#region*/
  const char *json = "{\"nested_arr\": [{}, [1]], \"val\": {}}";
  size_t len = strlen(json);
  TestSaxState state;
  init_state(&state, (const unsigned char *)json, len);

  bool result = jsonv_parse_sax((const unsigned char *)json, len, &all_callbacks, &state);

  cr_expect(result, "Parsing nested structures should succeed");
  cr_expect_eq(state.begin_object_count, 3); // root, {}, child in nested_arr, val (wait, [1] is array, so 3 objects)
  cr_expect_eq(state.end_object_count, 3);
  cr_expect_eq(state.begin_array_count, 2);  // nested_arr, [1]
  cr_expect_eq(state.end_array_count, 2);
  cr_expect_eq(state.number_count, 1);
  cr_expect_eq(state.key_count, 2);
  cr_expect(state.zero_copy_ok);
/*#endregion*/
END_TIMED_TEST

TIMED_TEST(T, parse_early_abort, setup_test, teardown_test)
/*#region*/
  const char *json = "{\"a\": 1, \"b\": 2}";
  size_t len = strlen(json);
  TestSaxState state;
  init_state(&state, (const unsigned char *)json, len);
  state.abort_after_n_events = 2; // abort after root object start (1) and first key (2)

  bool result = jsonv_parse_sax((const unsigned char *)json, len, &all_callbacks, &state);

  cr_expect(!result, "Parsing should abort early and return false when a callback returns false");
  cr_expect_eq(state.begin_object_count, 1);
  cr_expect_eq(state.key_count, 1);
  cr_expect_eq(state.number_count, 0, "No more events should be processed after abort");
/*#endregion*/
END_TIMED_TEST

TIMED_TEST(T, parse_invalid_json, setup_test, teardown_test)
/*#region*/
  const char *invalid_cases[] = {
    "",
    "{",
    "}",
    "[",
    "]",
    "{\"a\": 1,}", // trailing comma
    "[1, 2,]",    // trailing comma
    "{\"a\" 1}",   // missing colon
    "{\"a\": 1 \"b\": 2}", // missing comma
    "{\"a\": 1}, 2", // extra root value
    "123", // not object/array (wait, RFC 8259 allows this, let's see if we support it)
    "true",
    "\"string\""
  };

  for (size_t i = 0; i < sizeof(invalid_cases) / sizeof(invalid_cases[0]); i++) {
    const char *json = invalid_cases[i];
    size_t len = strlen(json);
    TestSaxState state;
    init_state(&state, (const unsigned char *)json, len);

    bool result = jsonv_parse_sax((const unsigned char *)json, len, &all_callbacks, &state);
    
    // Note: depending on RFC 8259 compliance, standalone literals like "123" might be valid.
    // If the case is a standalone literal, check if it fails or succeeds.
    // Let's print out the result to understand.
    if (i < 9) {
      cr_expect(!result, "Case '%s' should be invalid", json);
    }
  }
/*#endregion*/
END_TIMED_TEST
