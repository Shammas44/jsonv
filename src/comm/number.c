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

  // Skip leading whitespace
  while (isspace((unsigned char)*s))
    s++;

  if (*s == '\0')
    return false; // string is empty or only spaces

  errno = 0;
  char *end;
  long val = strtol(s, &end, 10);

  // Nothing converted?
  if (end == s)
    return false;

  // Skip trailing whitespace
  while (isspace((unsigned char)*end))
    end++;

  // Trailing non-space characters?
  if (*end != '\0')
    return false;

  // Range check
  if (errno == ERANGE || val < INT_MIN || val > INT_MAX)
    return false;

  *out = (int)val;
  return true;
/*#endregion*/
}
