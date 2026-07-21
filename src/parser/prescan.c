#include "prescan.h"
#include "except.h"
#include <string.h>

extern const Except MALFORMED_JSON;

void prescan(const char *s, size_t len, JsonEstimate *out) {
  /*#region*/
  if (len > 0xFFFFFFFF) {
    RAISE(MALFORMED_JSON);
  }

  memset(out, 0, sizeof(*out));

  bool in_string = false;
  bool escape = false;

  size_t start_i = 0;
  if (len >= 3 && (unsigned char)s[0] == 0xEF && (unsigned char)s[1] == 0xBB && (unsigned char)s[2] == 0xBF) {
    start_i = 3;
  }

  for (size_t i = start_i; i < len; i++) {
    char c = s[i];

    if (in_string) {
      if (escape) {
        escape = false;
        out->string_bytes++;
        continue;
      }
      if (c == '\\') {
        escape = true;
        continue;
      }
      if (c == '"') {
        in_string = false;
        out->value_count++; // string value
        continue;
      }
      out->string_bytes++;
      continue;
    }

    switch (c) {
    case '"':
      in_string = true;
      break;

    case '{':
      out->object_count++;
      out->cur_depth++;
      if (out->cur_depth > out->max_depth)
        out->max_depth = out->cur_depth;
      break;

    case '[':
      out->array_count++;
      out->cur_depth++;
      if (out->cur_depth > out->max_depth)
        out->max_depth = out->cur_depth;
      break;

    case '}':
    case ']':
      if (out->cur_depth > 0)
        out->cur_depth--;
      break;

    case 't': // true
    case 'f': // false
    case 'n': // null
    case '-': // number
    case '0' ... '9':
      out->value_count++;
      break;
    }
  }

  if (in_string)
    RAISE(MALFORMED_JSON);
  /*#endregion*/
}
