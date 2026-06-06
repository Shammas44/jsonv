# JSON Schema Validation VM: Instruction Set Architecture (ISA) Manual

This document serves as the formal Instruction Set Architecture (ISA) reference manual for the JSON Schema validation virtual machine.

---

## 1. Execution Model

The Validation VM is an **absolute-addressed, register-less virtual machine** designed to evaluate JSON data nodes against a serialized bytecode stream.

### 1.1 State and Recursion
* **Program Counter (`pc`)**: Points to the absolute byte index in the active bytecode buffer.
* **Context**: Validation state is tracked via a `Jsonv_Context`, which contains the AST node pool of the data payload and an error container (`E`).
* **Evaluation Semantics**: Validation starts at a specified bytecode `offset` on a target data AST node index (`node_idx`).
* **Subprogram Execution**: Nested schemas (for array elements or object properties) are executed via recursive calls to the validation interpreter with a new `offset` and target data `node_idx`.
* **Termination**: Execution terminates successfully when `OP_END` is decoded, or fails immediately if an assertion fails or `OP_FAIL` is decoded.

---

## 2. Binary Encoding & Data Layout

Instructions are variable-length and byte-aligned. Opcode decoding is performed using unaligned reads (via `memcpy`), which avoids CPU alignment traps on architectures like ARM and Apple Silicon.

### 2.1 Basic Types
* **`uint8_t`**: 1-byte unsigned integer. Used for Opcodes.
* **`int32_t` / `uint32_t`**: 4-byte signed/unsigned little-endian integers.
* **`double`**: 8-byte IEEE 754 double-precision float.
* **`Token`**: 24-byte serialized structure mapping to the JSON parser token:
  ```
  +---------------------------------------+
  |  TokenValue (union)        - 16 bytes |
  |    - double number (8 bytes)          |
  |    - ptr start / length (16 bytes)    |
  +---------------------------------------+
  |  TokenType (enum)          -  4 bytes |
  +---------------------------------------+
  |  Alignment Padding         -  4 bytes |
  +---------------------------------------+
  ```

---

## 3. Instruction Set Specification

### 3.0 OP_END (0x00)
Successful end of the current schema evaluation block.
```
+--------------+
| OP_END (0x00)|
+--------------+
```
* **Format**: `[0x00]` (1 byte)
* **VM Semantics**: Stops execution of the current VM block and returns `true`.

---

### 3.1 OP_FAIL (0x01)
Unconditional validation failure. Used for schemas defined as `false`.
```
+--------------+
|OP_FAIL (0x01)|
+--------------+
```
* **Format**: `[0x01]` (1 byte)
* **VM Semantics**: Immediately stops execution, writes a `Jsonv_ValueNotAllowed_error` to the error state, and returns `false`.

---

### 3.2 OP_TYPE (0x02)
Enforces type constraints on the current data node.
```
+--------------+--------------------------+
|OP_TYPE (0x02)|   type_mask (uint32_t)   |
+--------------+--------------------------+
```
* **Format**: `[0x02] [4 bytes type_mask]` (5 bytes)
* **Mask Flags**:
  * `0x01`: `null`
  * `0x02`: `boolean`
  * `0x04`: `number`
  * `0x08`: `string`
  * `0x10`: `array`
  * `0x20`: `object`
  * `0x40`: `integer` (matches numeric AST nodes with no fractional component)
* **VM Semantics**: If the current data AST node type is not set in `type_mask`, validation aborts with a `Jsonv_Type_error`.

---

### 3.3 OP_MINIMUM (0x03)
Asserts numeric values are greater than or equal to a lower bound.
```
+-----------------+---------------------------+
|OP_MINIMUM (0x03)|     min_val (double)      |
+-----------------+---------------------------+
```
* **Format**: `[0x03] [8 bytes min_val]` (9 bytes)
* **VM Semantics**: If the current data node is a number and its value is $< \text{min\_val}$, validation aborts with a `Jsonv_Minimum_error`. Non-numeric nodes ignore this instruction.

---

### 3.4 OP_MAXIMUM (0x04)
Asserts numeric values are less than or equal to an upper bound.
```
+-----------------+---------------------------+
|OP_MAXIMUM (0x04)|     max_val (double)      |
+-----------------+---------------------------+
```
* **Format**: `[0x04] [8 bytes max_val]` (9 bytes)
* **VM Semantics**: If the current data node is a number and its value is $> \text{max\_val}$, validation aborts with a `Jsonv_Maximum_error`. Non-numeric nodes ignore this instruction.

---

### 3.5 OP_MIN_LENGTH (0x05)
Asserts string length is greater than or equal to a minimum length.
```
+--------------------+---------------------------+
|OP_MIN_LENGTH (0x05)|     min_len (int32_t)     |
+--------------------+---------------------------+
```
* **Format**: `[0x05] [4 bytes min_len]` (5 bytes)
* **VM Semantics**: If the current data node is a string and its length in bytes is $< \text{min\_len}$, validation aborts with a `Jsonv_MinLength_error`. Non-string nodes ignore this instruction.

---

### 3.6 OP_MAX_LENGTH (0x06)
Asserts string length is less than or equal to a maximum length.
```
+--------------------+---------------------------+
|OP_MAX_LENGTH (0x06)|     max_len (int32_t)     |
+--------------------+---------------------------+
```
* **Format**: `[0x06] [4 bytes max_len]` (5 bytes)
* **VM Semantics**: If the current data node is a string and its length in bytes is $> \text{max\_len}$, validation aborts with a `Jsonv_MaxLength_error`. Non-string nodes ignore this instruction.

---

### 3.7 OP_MIN_ITEMS (0x07)
Asserts array element count is greater than or equal to a minimum size.
```
+-------------------+----------------------------+
|OP_MIN_ITEMS (0x07)|     min_items (int32_t)    |
+-------------------+----------------------------+
```
* **Format**: `[0x07] [4 bytes min_items]` (5 bytes)
* **VM Semantics**: If the current data node is an array and contains fewer than `min_items` elements, validation aborts with a `Jsonv_MinItems_error`. Non-array nodes ignore this instruction.

---

### 3.8 OP_MAX_ITEMS (0x08)
Asserts array element count is less than or equal to a maximum size.
```
+-------------------+----------------------------+
|OP_MAX_ITEMS (0x08)|     max_items (int32_t)    |
+-------------------+----------------------------+
```
* **Format**: `[0x08] [4 bytes max_items]` (5 bytes)
* **VM Semantics**: If the current data node is an array and contains more than `max_items` elements, validation aborts with a `Jsonv_MinItems_error`. Non-array nodes ignore this instruction.

---

### 3.9 OP_ITEMS (0x09)
Validates all elements in an array against a subschema.
```
+---------------+--------------------------+
|OP_ITEMS (0x09)|     offset (uint32_t)    |
+---------------+--------------------------+
```
* **Format**: `[0x09] [4 bytes offset]` (5 bytes)
* **VM Semantics**: If the current data node is an array, the interpreter loops through all children elements and recursively executes `validate_bytecode` using the target `offset`. If any element fails, validation aborts.

---

### 3.A OP_REQUIRED (0x0A)
Asserts that specified object keys are present.
```
+-----------------+--------------------+--------------------------------+
|OP_REQUIRED(0x0A)|  count (uint32_t)  |    tokens[count] (Token[])     |
+-----------------+--------------------+--------------------------------+
```
* **Format**: `[0x0A] [4 bytes count] [count * 24 bytes Tokens]`
* **VM Semantics**: If the current data node is an object, the VM verifies that each of the `count` property keys is defined on the object. If any property is missing, validation aborts with a `Jsonv_Required_error`.

---

### 3.B OP_PROPERTIES (0x0B)
Handles object property keys validation and additional properties enforcement.
```
+-------------------+-------------------+------------------------+
|OP_PROPERTIES(0x0B)|  count (uint32_t)  |  add_offset (int32_t)  |
+-------------------+-------------------+------------------------+
|                      (Token key, uint32_t offset)[count]       |
+----------------------------------------------------------------+
```
* **Format**: `[0x0B] [4 bytes count] [4 bytes add_offset] [count * 28 bytes key/offset entries]`
* **VM Semantics**: If the current data node is an object, the VM processes each property field:
  1. Matches the field key string against the list of `count` property keys.
  2. If matched, recursively validates the property value node against the subschema at the corresponding `offset`.
  3. If unmatched (i.e. it is an additional property):
     * If `add_offset == -2`, validation immediately aborts with `Jsonv_AdditionalProperties_error`.
     * If `add_offset >= 0`, recursively validates the value node against the subschema at the absolute bytecode `add_offset`.
     * If `add_offset == -1`, the property is allowed without validation.

---

### 3.C OP_MULTIPLE_OF (0x0C)
Asserts that a numeric value is a multiple of a given divisor.
```
+--------------------+------------------------------+
| OP_MULTIPLE_OF(0x0C)|     divisor (double)         |
+--------------------+------------------------------+
```
* **Format**: `[0x0C] [8 bytes divisor]` (9 bytes)
* **VM Semantics**: If the current data node is a number, the VM asserts that the number is an integer multiple of `divisor` (within a floating-point tolerance of $10^{-9}$). If not, validation aborts with a `Jsonv_MultipleOf_error`. Non-numeric data nodes ignore this instruction.

---

### 3.D OP_EXCLUSIVE_MINIMUM (0x0D)
Asserts numeric values are strictly greater than a lower bound.
```
+---------------------------+---------------------------------+
| OP_EXCLUSIVE_MINIMUM(0x0D)|     min_val (double)            |
+---------------------------+---------------------------------+
```
* **Format**: `[0x0D] [8 bytes min_val]` (9 bytes)
* **VM Semantics**: If the current data node is a number and its value is $\le \text{min\_val}$, validation aborts with a `Jsonv_ExclusiveMinimum_error`. Non-numeric nodes ignore this instruction.

---

### 3.E OP_EXCLUSIVE_MAXIMUM (0x0E)
Asserts numeric values are strictly less than an upper bound.
```
+---------------------------+---------------------------------+
| OP_EXCLUSIVE_MAXIMUM(0x0E)|     max_val (double)            |
+---------------------------+---------------------------------+
```
* **Format**: `[0x0E] [8 bytes max_val]` (9 bytes)
* **VM Semantics**: If the current data node is a number and its value is $\ge \text{max\_val}$, validation aborts with a `Jsonv_ExclusiveMaximum_error`. Non-numeric nodes ignore this instruction.


