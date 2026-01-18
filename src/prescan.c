#include "prescan.h"
#include "except.h"
#include <string.h>

extern const Except MALFORMED_JSON;

void jsonv_prescan(const char *s, size_t len, JsonEstimate *out) {
  /*#region*/
  memset(out, 0, sizeof(*out));

  bool in_string = false;
  bool escape = false;

  for (size_t i = 0; i < len; i++) {
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
