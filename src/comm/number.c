#include "number.h"
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stdlib.h>

bool parse_int(const char *s, int *out) {
  /*#region*/
  if (!s || !out)
    return false;

  while (isspace((unsigned char)*s))
    s++;

  if (*s == '\0')
    return false;

  const char *orig = s;

  // --- Handle optional sign ---
  // bool negative = false;
  if (*s == '+' || *s == '-') {
    // negative = (*s == '-');
    s++;
  }

  // --- Leading zero check ---
  if (*s == '0') {
    // "0" is allowed, but "0X"/"01"/"00" are NOT.
    if (s[1] != '\0' && !isspace((unsigned char)s[1]))
      return false;
  }

  errno = 0;
  char *end = NULL;
  long val = strtol(orig, &end, 10);

  if (end == orig)
    return false;

  while (isspace((unsigned char)*end))
    end++;

  if (*end != '\0')
    return false;

  if (errno == ERANGE || val < INT_MIN || val > INT_MAX)
    return false;

  if (out)
    *out = (int)val;
  return true;
  /*#endregion*/
}

bool parse_double(const char *s, double *out) {
  /*#region*/
  if (!s || !out)
    return false;

  while (isspace((unsigned char)*s))
    s++;

  if (*s == '\0')
    return false;

  const char *orig = s;

  // bool negative = false;
  if (*s == '+' || *s == '-') {
    // negative = (*s == '-');
    s++;
  }

  // --- Leading zero rule ---
  if (*s == '0') {
    // Allowed forms: "0", "0.xxx", "0e10"
    if (s[1] != '\0' && !isspace((unsigned char)s[1]) && s[1] != '.' &&
        s[1] != 'e' && s[1] != 'E') {
      return false; // Reject "01", "05", "0007"
    }
  }

  errno = 0;
  char *endptr = NULL;
  double value = strtod(orig, &endptr);

  if (endptr == orig)
    return false;

  if (errno == ERANGE)
    return false;

  while (isspace((unsigned char)*endptr))
    endptr++;

  if (*endptr != '\0')
    return false;

  if (out)
    *out = value;
  return true;
  /*#endregion*/
}
