#include "validate_internal.h"

bool validate_bytecode(
    Jsonv_Context *ctx,
    ASTNode *pool,
    const Jsonv_Schema *schema,
    uint32_t offset,
    int node_idx,
    const char *path,
    E *out_err
) {
  /*#region*/
  if (offset >= schema->length) return true;

  BytecodeHeader header;
  if (schema->length < sizeof(BytecodeHeader)) {
    out_err->type = Jsonv_Malformed_json;
    snprintf(out_err->description, sizeof(out_err->description), "Invalid bytecode: length too small");
    return false;
  }
  memcpy(&header, schema->bytecode, sizeof(BytecodeHeader));
  if (offset == sizeof(BytecodeHeader)) {
    if (header.magic != 0x4A535642 || header.version != 1) {
      out_err->type = Jsonv_Malformed_json;
      snprintf(out_err->description, sizeof(out_err->description), "Invalid bytecode: magic/version mismatch");
      return false;
    }
  }

  const uint8_t *constant_pool = schema->bytecode + sizeof(BytecodeHeader) + header.code_size;
  const uint8_t *pc = schema->bytecode + offset;

  bool running = true;
  while (running) {
    uint8_t opcode = read_byte(&pc);
    if (opcode == OP_END) {
      running = false;
      break;
    }

    if (opcode >= sizeof(opcode_handlers) / sizeof(opcode_handlers[0])) {
      return false;
    }

    OpcodeHandler handler = opcode_handlers[opcode];
    if (handler) {
      if (!handler(ctx, pool, schema, &pc, node_idx, path, out_err, constant_pool)) {
        return false;
      }
    } else {
      return false;
    }
  }

  return true;
  /*#endregion*/
}
