#include "hint.h"
#include "token.h"
#include <string.h>

void hint_to_char(const char *json, jsonv_Hint hint, char *buff) {
  /*#region*/
  int len = hint.end - hint.start;
  strncpy(buff, json + hint.start, len);
  buff[len] = '\0';
  /*#endregion*/
}

void tok_to_char(Token tok, char *buff) {
  /*#region*/
  int len = tok.length;
  strncpy(buff, tok.start, len);
  buff[len] = '\0';
  /*#endregion*/
}
