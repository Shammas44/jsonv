#include "token.h"
#include "assert.h"
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#define T jsonv_tokiterator

typedef struct T {
  jsmntok_t *tokens;
  int length;
  int index;
  const char *json;
} T;

T *jsonv_tokiterator_new(const char *json, jsmntok_t **tokens, int tok_count) {
  /*#region*/
  if (tok_count <= 1)
    return NULL;
  T *self = malloc(sizeof(T));
  memset(self, 0, sizeof(T));
  self->tokens = *tokens;
  self->length = tok_count;
  self->json = json;
  return self;
  /*#endregion*/
}

jsmntok_t *jsonv_tokiterator_next(T *self) {
  /*#region*/
  assert(self);
  if (self->index + 1 == self->length)
    return NULL;
  self->index += 1;
  return &self->tokens[self->index];
  /*#endregion*/
}

jsmntok_t *jsonv_tokiterator_current(T *self) {
  /*#region*/
  assert(self);
  return &self->tokens[self->index];
  /*#endregion*/
}

jsmntok_t *jsonv_tokiterator_relative(T *self, int index) {
  /*#region*/
  assert(self);
  assert(self->index + index <= self->length);
  assert(self->index + index >= 0);
  return &self->tokens[self->index + index];
  /*#endregion*/
}

int jsonv_tokiterator_index(T *self) {
  /*#region*/
  assert(self);
  return self->index;
  /*#endregion*/
}

void jsonv_tokiterator_free(T **self) {
  /*#region*/
  assert(self);
  free(*self);
  self = NULL;
  /*#endregion*/
}

/**
 * Extracts and copies the string content from a token, handling quoting.
 * The returned string is NULL-terminated and must be freed by the caller.
 * @param json The raw JSON schema string.
 * @param tokens The JSMN token array.
 * @param i The index of the token containing the string value (key or value).
 * @return A newly allocated, null-terminated string, or NULL on error.
 */
char *jsonv_extract_token_string(const char *json, const jsmntok_t *token) {
  /*#region*/
  // Accept JSMN_STRING (quoted values like "string") or JSMN_PRIMITIVE
  // (unquoted keywords like "true", "null")
  if (token->type != JSMN_STRING && token->type != JSMN_PRIMITIVE) {
    return NULL;
  }

  size_t length = token->end - token->start;

  // Allocate memory for the string plus the null terminator
  char *str = (char *)malloc(length + 1);
  if (str == NULL) {
    return NULL; // Allocation failure
  }

  // Copy the substring from the raw JSON data
  strncpy(str, json + token->start, length);
  str[length] = '\0'; // Null-terminate the string

  // In a full implementation, you would handle JSON escape sequences here
  // (e.g., \n, \t, \uXXXX) JSMN doesn't handle unescaping, so a simple strncpy
  // is sufficient for basic schema keywords.

  return str;
  /*#endregion*/
}

/**
 * Parses the token at index 'i' to extract a C integer/number.
 * Assumes the token points to a number in the JSON string.
 * @param json The raw JSON schema string.
 * @param tokens The JSMN token array.
 * @param i The index of the token containing the integer value.
 * @return The parsed integer value, or -1 if the token is invalid or parsing
 * fails.
 */
int jsonv_extract_token_int(const char *json, const jsmntok_t *token) {
  /*#region*/
  if (token->type != JSMN_PRIMITIVE && token->type != JSMN_STRING) {
    return -1; // Not a primitive or string (e.g., object, array, null)
  }

  const char *start = json + token->start;
  size_t length = token->end - token->start;

  // We need a null-terminated string for strtol.
  // Use a small buffer to copy the segment.
  char buffer[32]; // Sufficient for most integer limits

  if (length > sizeof(buffer) - 1) {
    // The number is too long for the buffer, indicating a potential issue or
    // huge number. For simplicity, we return an error. A production library
    // might use dynamic allocation.
    return -1;
  }

  strncpy(buffer, start, length);
  buffer[length] = '\0'; // Null-terminate the string

  // Use strtol for robust conversion and error checking
  char *endptr;
  errno = 0; // Clear errno before the call
  long value = strtol(buffer, &endptr, 10);

  // Check for errors (e.g., non-numeric data, overflow)
  if (errno == ERANGE || endptr == buffer || *endptr != '\0') {
    return -1; // Conversion failed
  }

  // Check if the resulting long fits within the target int
  if (value > 2147483647L ||
      value < -2147483648L) { // INT_MAX and INT_MIN limits
    return -1;                // Value out of range for int
  }

  return (int)value;
  /*#endregion*/
}
