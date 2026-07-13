#ifndef _JSONV_YAML_LEXER_H_INCLUDED
#define _JSONV_YAML_LEXER_H_INCLUDED

#ifdef JSONV_YAML_SUPPORT

#include "token.h"
#include <stddef.h>
#include <stdbool.h>

#define MAX_YAML_DEPTH 64

typedef struct YamlLexer {
  const unsigned char *source;
  size_t source_len;
  size_t current_pos;

  // Indentation stack
  int indent_stack[MAX_YAML_DEPTH];
  int indent_top;

  // State to yield virtual indents/dedents
  int pending_dedents;
  bool emit_indent;
  
  // Positional and scanning states
  bool is_line_start;
  int current_line;
  int current_col;

  // Token buffer for virtual tokens or lookahead
  Token queued_token;
  bool has_queued_token;

  // Flow style nesting tracker
  int flow_depth;
} YamlLexer;

void yaml_lexer_init(YamlLexer *l, const unsigned char *source, size_t len);
Token yaml_lexer_next_token(YamlLexer *l);

#endif // JSONV_YAML_SUPPORT

#endif

