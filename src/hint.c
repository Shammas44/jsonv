#include "hint.h"
#include "jsmn.h"
#include <string.h>

void hint_to_char(const char *json, jsonv_Hint hint, char *buff) {
  /*#region*/
  int len = hint.end - hint.start;
  strncpy(buff, json + hint.start, len);
  buff[len] = '\0';
  /*#endregion*/
}

void jsmntok_to_char(const char *json, jsmntok_t tok, char *buff) {
  /*#region*/
  int len = tok.end - tok.start;
  strncpy(buff, json + tok.start, len);
  buff[len] = '\0';
  /*#endregion*/
}
