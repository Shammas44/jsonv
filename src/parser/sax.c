#include "sax.h"
#include "lexer.h"
#include "token.h"
#include <string.h>

#define MAX_SAX_DEPTH 64

typedef struct {
  Lexer lexer;
  Token current_tok;
  const unsigned char *token_start;
  const unsigned char *token_end;
} SaxParser;

static Token sax_next_token(Lexer *l, const unsigned char **out_start, const unsigned char **out_end) {
  // Optimization: Fast Lookup Table for whitespace scanning
  static const bool is_space_lut[256] = {
      [' '] = true, ['\t'] = true, ['\r'] = true, ['\n'] = true
  };
  while (l->current_pos < l->source_len && is_space_lut[l->source[l->current_pos]]) {
    l->current_pos++;
  }

  *out_start = l->source + l->current_pos;

  Token tok = lexer_next_token(l);

  *out_end = l->source + l->current_pos;

  return tok;
}

static bool sax_advance(SaxParser *p) {
  p->current_tok = sax_next_token(&p->lexer, &p->token_start, &p->token_end);
  return p->current_tok.type != T_ERROR;
}

static bool sax_parse_value(SaxParser *p, const jsonv_sax_callbacks *callbacks, void *user_data, int depth);
static bool sax_parse_object(SaxParser *p, const jsonv_sax_callbacks *callbacks, void *user_data, int depth);
static bool sax_parse_array(SaxParser *p, const jsonv_sax_callbacks *callbacks, void *user_data, int depth);

static bool sax_parse_value(SaxParser *p, const jsonv_sax_callbacks *callbacks, void *user_data, int depth) {
  if (depth > MAX_SAX_DEPTH) {
    return false; // Maximum depth reached
  }

  switch (p->current_tok.type) {
    case T_NULL:
      if (callbacks && callbacks->on_null) {
        if (!callbacks->on_null(user_data)) return false;
      }
      return sax_advance(p);

    case T_TRUE:
      if (callbacks && callbacks->on_boolean) {
        if (!callbacks->on_boolean(true, user_data)) return false;
      }
      return sax_advance(p);

    case T_FALSE:
      if (callbacks && callbacks->on_boolean) {
        if (!callbacks->on_boolean(false, user_data)) return false;
      }
      return sax_advance(p);

    case T_NUMBER:
      if (callbacks && callbacks->on_number) {
        size_t len = p->token_end - p->token_start;
        if (!callbacks->on_number(p->token_start, len, user_data)) return false;
      }
      return sax_advance(p);

    case T_STRING:
      if (callbacks && callbacks->on_string) {
        if (!callbacks->on_string(p->current_tok.value.string.start, p->current_tok.value.string.length, user_data)) return false;
      }
      return sax_advance(p);

    case T_BRACE_OPEN:
      return sax_parse_object(p, callbacks, user_data, depth);

    case T_BRACKET_OPEN:
      return sax_parse_array(p, callbacks, user_data, depth);

    default:
      return false; // Malformed JSON value
  }
}

static bool sax_parse_object(SaxParser *p, const jsonv_sax_callbacks *callbacks, void *user_data, int depth) {
  if (callbacks && callbacks->on_begin_object) {
    if (!callbacks->on_begin_object(user_data)) return false;
  }

  if (!sax_advance(p)) return false;

  bool first = true;
  while (p->current_tok.type != T_BRACE_CLOSE) {
    if (!first) {
      if (p->current_tok.type != T_COMMA) return false; // Expected comma
      if (!sax_advance(p)) return false;
      // Trailing comma check
      if (p->current_tok.type == T_BRACE_CLOSE) return false;
    }
    first = false;

    if (p->current_tok.type != T_STRING) return false; // Key must be a string

    if (callbacks && callbacks->on_object_key) {
      if (!callbacks->on_object_key(p->current_tok.value.string.start, p->current_tok.value.string.length, user_data)) return false;
    }

    if (!sax_advance(p)) return false;

    if (p->current_tok.type != T_COLON) return false; // Expected colon
    if (!sax_advance(p)) return false;

    if (!sax_parse_value(p, callbacks, user_data, depth + 1)) return false;
  }

  if (callbacks && callbacks->on_end_object) {
    if (!callbacks->on_end_object(user_data)) return false;
  }

  return sax_advance(p);
}

static bool sax_parse_array(SaxParser *p, const jsonv_sax_callbacks *callbacks, void *user_data, int depth) {
  if (callbacks && callbacks->on_begin_array) {
    if (!callbacks->on_begin_array(user_data)) return false;
  }

  if (!sax_advance(p)) return false;

  bool first = true;
  while (p->current_tok.type != T_BRACKET_CLOSE) {
    if (!first) {
      if (p->current_tok.type != T_COMMA) return false; // Expected comma
      if (!sax_advance(p)) return false;
      // Trailing comma check
      if (p->current_tok.type == T_BRACKET_CLOSE) return false;
    }
    first = false;

    if (!sax_parse_value(p, callbacks, user_data, depth + 1)) return false;
  }

  if (callbacks && callbacks->on_end_array) {
    if (!callbacks->on_end_array(user_data)) return false;
  }

  return sax_advance(p);
}

bool jsonv_parse_sax(
    const unsigned char *buffer,
    size_t length,
    const jsonv_sax_callbacks *callbacks,
    void *user_data
) {
  if (!buffer || length == 0) {
    return false;
  }

  SaxParser parser;
  lexer_init(&parser.lexer, buffer, length);

  if (!sax_advance(&parser)) {
    return false;
  }

  if (!sax_parse_value(&parser, callbacks, user_data, 0)) {
    return false;
  }

  // Ensure no trailing garbage tokens (ignoring whitespace which is skipped by sax_advance/EOF check)
  if (parser.current_tok.type != T_EOF) {
    return false;
  }

  return true;
}
