#include "unescape.h"
#include <stdint.h>
#include <stdbool.h>

static inline bool parse_4hex(const unsigned char *p, uint16_t *out) {
  /*#region*/
  uint16_t v = 0;
  for (int i = 0; i < 4; i++) {
    unsigned char c = p[i];
    if (c >= '0' && c <= '9') v = (v << 4) | (c - '0');
    else if (c >= 'a' && c <= 'f') v = (v << 4) | (c - 'a' + 10);
    else if (c >= 'A' && c <= 'F') v = (v << 4) | (c - 'A' + 10);
    else return false;
  }
  *out = v;
  return true;
  /*#endregion*/
}

static inline size_t encode_utf8(uint32_t cp, char *dest) {
  /*#region*/
  if (cp <= 0x7F) {
    dest[0] = (char)cp;
    return 1;
  } else if (cp <= 0x7FF) {
    dest[0] = (char)(0xC0 | (cp >> 6));
    dest[1] = (char)(0x80 | (cp & 0x3F));
    return 2;
  } else if (cp <= 0xFFFF) {
    dest[0] = (char)(0xE0 | (cp >> 12));
    dest[1] = (char)(0x80 | ((cp >> 6) & 0x3F));
    dest[2] = (char)(0x80 | (cp & 0x3F));
    return 3;
  } else {
    dest[0] = (char)(0xF0 | (cp >> 18));
    dest[1] = (char)(0x80 | ((cp >> 12) & 0x3F));
    dest[2] = (char)(0x80 | ((cp >> 6) & 0x3F));
    dest[3] = (char)(0x80 | (cp & 0x3F));
    return 4;
  }
  /*#endregion*/
}

size_t jsonv_unescape_string(const unsigned char *src, size_t len, char *dest) {
  /*#region*/
  size_t r = 0;
  size_t w = 0;

  while (r < len) {
    unsigned char c = src[r];
    if (c != '\\') {
      dest[w++] = (char)c;
      r++;
      continue;
    }

    // Encountered '\'
    r++; // skip '\'
    if (r >= len) break;
    unsigned char esc = src[r++];
    switch (esc) {
      case '"':  dest[w++] = '"';  break;
      case '\\': dest[w++] = '\\'; break;
      case '/':  dest[w++] = '/';  break;
      case 'b':  dest[w++] = '\b'; break;
      case 'f':  dest[w++] = '\f'; break;
      case 'n':  dest[w++] = '\n'; break;
      case 'r':  dest[w++] = '\r'; break;
      case 't':  dest[w++] = '\t'; break;
      case 'u': {
        if (r + 4 <= len) {
          uint16_t u1 = 0;
          if (parse_4hex(src + r, &u1)) {
            r += 4;
            if (u1 >= 0xD800 && u1 <= 0xDBFF) {
              // High surrogate, look for low surrogate \uDC00..\uDFFF
              if (r + 6 <= len && src[r] == '\\' && src[r + 1] == 'u') {
                uint16_t u2 = 0;
                if (parse_4hex(src + r + 2, &u2) && u2 >= 0xDC00 && u2 <= 0xDFFF) {
                  r += 6;
                  uint32_t codepoint = 0x10000 + (((uint32_t)(u1 - 0xD800) << 10) | (u2 - 0xDC00));
                  w += encode_utf8(codepoint, dest + w);
                  break;
                }
              }
            }
            w += encode_utf8(u1, dest + w);
            break;
          }
        }
        dest[w++] = 'u';
        break;
      }
      default:
        dest[w++] = (char)esc;
        break;
    }
  }

  dest[w] = '\0';
  return w;
  /*#endregion*/
}
