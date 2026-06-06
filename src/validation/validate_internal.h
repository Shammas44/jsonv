#ifndef _JSONV_VALIDATE_INTERNAL_H
#define _JSONV_VALIDATE_INTERNAL_H

#include "validate.h"
#include "shape.internal.h"
#include "schema.h"
#include "ctx.h"
#include "parser.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <regex.h>

typedef struct {
  const char *key_start;
  uint32_t key_len;
  uint32_t rule_offset;
} DecodedPropertyRule;

typedef struct {
  Jsonv_Context *ctx;
  ASTNode *pool;
  const Jsonv_Schema *schema;
  const uint8_t *pc;
  int node_idx;
  const char *path;
  E *out_err;
  const uint8_t *constant_pool;
} VMState;

/* Bytecode Reading Helpers */
static inline uint8_t read_byte(const uint8_t **pc) {
  /*#region*/
  uint8_t val = **pc;
  (*pc)++;
  return val;
  /*#endregion*/
}

static inline uint32_t read_uint32(const uint8_t **pc) {
  /*#region*/
  uint32_t val;
  memcpy(&val, *pc, sizeof(val));
  *pc += sizeof(val);
  return val;
  /*#endregion*/
}

static inline int32_t read_int32(const uint8_t **pc) {
  /*#region*/
  int32_t val;
  memcpy(&val, *pc, sizeof(val));
  *pc += sizeof(val);
  return val;
  /*#endregion*/
}

static inline double read_double(const uint8_t **pc) {
  /*#region*/
  double val;
  memcpy(&val, *pc, sizeof(val));
  *pc += sizeof(val);
  return val;
  /*#endregion*/
}

/* Helper functions defined in validate_helpers.c */
bool regex_matches_key(const char *k_start, size_t k_len, const char *pat_start, size_t pat_len, Jsonv_Context *ctx, const char *path, E *out_err);
bool validate_ipv4(const char *s, size_t len);
bool validate_email(const char *s, size_t len);
bool validate_uuid(const char *s, size_t len);
bool validate_datetime(const char *s, size_t len);
bool ast_nodes_equal(const ASTNode *pool, int n1_idx, int n2_idx);

/* Handler signature for opcode execution */
typedef bool (*OpcodeHandler)(VMState *state);

extern const OpcodeHandler opcode_handlers[OP_FORMAT + 1];

#endif
