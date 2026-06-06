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

The compiled bytecode uses a structured ELF-style layout:

```
+--------------------+
|   BytecodeHeader   | (16 bytes)
+--------------------+
|    Code Section    | (variable, size = header.code_size)
+--------------------+
|   Constant Pool    | (variable, size = header.data_size)
+--------------------+
```

### 2.1 BytecodeHeader Layout
The bytecode starts with a 16-byte fixed-size header:
* **`magic`** (uint32_t, 4 bytes): Identifier magic number `0x4A535642` ("JSVB" in ASCII).
* **`version`** (uint32_t, 4 bytes): Format version, currently `1`.
* **`code_size`** (uint32_t, 4 bytes): Size in bytes of the following Code Section.
* **`data_size`** (uint32_t, 4 bytes): Size in bytes of the trailing Constant Pool.

### 2.2 Basic Types
* **`uint8_t`**: 1-byte unsigned integer. Used for Opcodes.
* **`int32_t` / `uint32_t`**: 4-byte signed/unsigned little-endian integers.
* **`double`**: 8-byte IEEE 754 double-precision float.
* **`StringRef`**: A reference to a string stored inside the Constant Pool:
  ```
  +---------------------------------------+
  |  offset (uint32_t)         -  4 bytes |
  +---------------------------------------+
  |  length (uint32_t)         -  4 bytes |
  +---------------------------------------+
  ```
  The string is stored at `constant_pool_start + offset` and has the specified `length`. Strings are not guaranteed to be null-terminated, so the length field must always be respected.

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
+-----------------+--------------------+------------------------------------+
|OP_REQUIRED(0x0A)|  count (uint32_t)  | StringRef keys[count] (8B each)   |
+-----------------+--------------------+------------------------------------+
```
* **Format**: `[0x0A] [4 bytes count] [count * 8 bytes StringRefs]` (1 + 4 + count * 8 bytes)
* **VM Semantics**: If the current data node is an object, the VM verifies that each of the `count` property keys is defined on the object. The keys are resolved from the Constant Pool via their `StringRef`. If any property is missing, validation aborts with a `Jsonv_Required_error`.

---

### 3.B OP_PROPERTIES (0x0B)
Handles object properties, patternProperties, and additionalProperties validation.
```
+-------------------+--------------------+---------------------+------------------------+
|OP_PROPERTIES(0x0B)|   count (uint32_t) | pat_count (uint32_t)|  add_offset (int32_t)  |
+-------------------+--------------------+---------------------+------------------------+
|                    (StringRef key, uint32_t offset)[count] (12B each)                 |
+---------------------------------------------------------------------------------------+
|                    (StringRef pattern, uint32_t offset)[pat_count] (12B each)         |
+---------------------------------------------------------------------------------------+
```
* **Format**: `[0x0B] [4 bytes count] [4 bytes pat_count] [4 bytes add_offset] [count * 12 bytes key/offset entries] [pat_count * 12 bytes pattern/offset entries]`
* **VM Semantics**: If the current data node is an object, the VM processes each property field:
  1. Matches the field key string against the list of `count` static property keys (resolved via `StringRef`). If matched, recursively validates the property value node against the subschema at the corresponding `offset`, and marks the key as matched.
  2. Matches the field key string against the list of `pat_count` regular expression patterns (resolved via `StringRef`). For each pattern that matches, recursively validates the property value node against the subschema at the corresponding `offset`, and marks the key as matched.
  3. If the key is unmatched by both static properties and patternProperties:
     * If `add_offset == -2`, validation immediately aborts with `Jsonv_AdditionalProperties_error`.
     * If `add_offset >= 0`, recursively validates the property value node against the subschema at the absolute bytecode `add_offset`.
     * If `add_offset == -1`, the property is allowed without validation.

---

### 3.C OP_MULTIPLE_OF (0x0C)
Asserts that a numeric value is a multiple of a given divisor.
```
+---------------------+------------------------------+
| OP_MULTIPLE_OF(0x0C)|     divisor (double)         |
+---------------------+------------------------------+
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

---

### 3.F OP_PATTERN (0x0F)
Asserts that a string instance matches a regular expression.
```
+----------------+------------------------------+
|OP_PATTERN(0x0F)|     pattern (StringRef)      |
+----------------+------------------------------+
```
* **Format**: `[0x0F] [8 bytes pattern StringRef]` (9 bytes)
* **VM Semantics**: If the current data node is a string, the VM compiles the regular expression in `pattern` (resolved from the Constant Pool) using POSIX Extended Regular Expressions (`REG_EXTENDED`) and matches the string against it. If it does not match, validation aborts with a `Jsonv_Pattern_error`. If regex compilation fails, it aborts with a `Jsonv_Compile_Regexp_Failed`. Non-string nodes ignore this instruction.

---

### 3.G OP_MIN_PROPERTIES (0x10)
Asserts that the number of properties in an object is greater than or equal to a minimum count.
```
+------------------------+-------------------------------+
| OP_MIN_PROPERTIES(0x10)|     min_props (int32_t)       |
+------------------------+-------------------------------+
```
* **Format**: `[0x10] [4 bytes min_props]` (5 bytes)
* **VM Semantics**: If the current data node is an object and contains fewer than `min_props` properties, validation aborts with a `Jsonv_MinProperties_error`. Non-object nodes ignore this instruction.

---

### 3.H OP_MAX_PROPERTIES (0x11)
Asserts that the number of properties in an object is less than or equal to a maximum count.
```
+------------------------+-------------------------------+
| OP_MAX_PROPERTIES(0x11)|     max_props (int32_t)       |
+------------------------+-------------------------------+
```
* **Format**: `[0x11] [4 bytes max_props]` (5 bytes)
* **VM Semantics**: If the current data node is an object and contains more than `max_props` properties, validation aborts with a `Jsonv_MaxProperties_error`. Non-object nodes ignore this instruction.

---

### 3.I OP_UNIQUE_ITEMS (0x12)
Asserts that all elements in an array are unique.
```
+----------------------+
| OP_UNIQUE_ITEMS(0x12)|
+----------------------+
```
* **Format**: `[0x12]` (1 byte)
* **VM Semantics**: If the current data node is an array, the VM recursively compares every element against all subsequent elements. If any two elements are equal, validation aborts with a `Jsonv_UniqueItems_error`. Non-array nodes ignore this instruction.

---

### 3.J OP_CONTAINS (0x13)
Asserts that at least one element in an array matches the subschema.
```
+------------------+---------------------------+
| OP_CONTAINS(0x13)|     offset (uint32_t)     |
+------------------+---------------------------+
```
* **Format**: `[0x13] [4 bytes offset]` (5 bytes)
* **VM Semantics**: If the current data node is an array, the interpreter loops through all children elements and recursively executes `validate_bytecode` using the target `offset`. If at least one element passes, the validation is successful. If no element passes or the array is empty, validation aborts with a `Jsonv_Contains_error`. Non-array nodes ignore this instruction.

---

### 3.K OP_NOT (0x14)
Asserts that the data instance does not validate against the subschema.
```
+---------------+---------------------------+
|  OP_NOT(0x14) |     offset (uint32_t)     |
+---------------+---------------------------+
```
* **Format**: `[0x14] [4 bytes offset]` (5 bytes)
* **VM Semantics**: The interpreter recursively validates the current data node against the subschema at the absolute bytecode `offset`. If the subschema validation returns `true` (validates successfully), validation aborts with a `Jsonv_Not_error`. If it returns `false`, validation succeeds.

---

### 3.L OP_ALL_OF (0x15)
Asserts that the data instance validates against all subschemas listed.
```
+----------------+--------------------+---------------------------------+
| OP_ALL_OF(0x15)|  count (uint32_t)  |    offsets[count] (uint32_t[])  |
+----------------+--------------------+---------------------------------+
```
* **Format**: `[0x15] [4 bytes count] [count * 4 bytes offsets]`
* **VM Semantics**: The interpreter recursively validates the current data node against each of the `count` subschemas at their corresponding absolute bytecode `offsets`. If all validations return `true`, the validation is successful. If any validation fails, validation aborts with a `Jsonv_AllOf_error`. All arguments are fully decoded sequentially from the instruction stream.

### 3.M OP_ANY_OF (0x16)
Asserts that the data instance validates against at least one subschema listed.
```
+----------------+--------------------+---------------------------------+
| OP_ANY_OF(0x16)|  count (uint32_t)  |    offsets[count] (uint32_t[])  |
+----------------+--------------------+---------------------------------+
```
* **Format**: `[0x16] [4 bytes count] [count * 4 bytes offsets]`
* **VM Semantics**: The interpreter recursively validates the current data node against each of the `count` subschemas at their corresponding absolute bytecode `offsets`. If at least one validation returns `true`, the validation is successful. If all validations fail, validation aborts with a `Jsonv_AnyOf_error`. All arguments are fully decoded sequentially from the instruction stream.

---

### 3.N OP_ONE_OF (0x17)
Asserts that the data instance validates against exactly one of the subschemas listed.
```
+----------------+--------------------+---------------------------------+
| OP_ONE_OF(0x17)|  count (uint32_t)  |    offsets[count] (uint32_t[])  |
+----------------+--------------------+---------------------------------+
```
* **Format**: `[0x17] [4 bytes count] [count * 4 bytes offsets]`
* **VM Semantics**: The interpreter recursively validates the current data node against each of the `count` subschemas at their corresponding absolute bytecode `offsets`. If exactly one validation returns `true`, the validation is successful. If zero or more than one subschemas validate successfully, validation aborts with a `Jsonv_OneOf_error`. All arguments are fully decoded sequentially from the instruction stream.

---

### 3.O OP_IF_THEN_ELSE (0x18)
Conditional application of subschemas.
```
+----------------------+----------------------+-----------------------+-----------------------+
| OP_IF_THEN_ELSE(0x18)|  if_offset (uint32_t)| then_offset (uint32_t)| else_offset (uint32_t)|
+----------------------+----------------------+-----------------------+-----------------------+
```
* **Format**: `[0x18] [4 bytes if_offset] [4 bytes then_offset] [4 bytes else_offset]`
* **VM Semantics**: Recursively evaluates the subschema at `if_offset` against the current data node.
  * If the `if` subschema validates successfully:
    * If `then_offset` is not `(uint32_t)-1` (denoting absent `then` schema), recursively evaluates the subschema at `then_offset`. The outcome of this validation determines the overall outcome.
    * If `then_offset` is `(uint32_t)-1`, validation succeeds immediately.
  * If the `if` subschema fails validation:
    * If `else_offset` is not `(uint32_t)-1` (denoting absent `else` schema), recursively evaluates the subschema at `else_offset`. The outcome of this validation determines the overall outcome.
    * If `else_offset` is `(uint32_t)-1`, validation succeeds immediately.

---

### 3.P OP_PROPERTY_NAMES (0x19)
Asserts that all property keys of an object validate against a subschema.
```
+------------------------+--------------------------+
| OP_PROPERTY_NAMES(0x19)|    offset (uint32_t)     |
+------------------------+--------------------------+
```
* **Format**: `[0x19] [4 bytes offset]` (5 bytes)
* **VM Semantics**: If the current data node is an object, the VM iterates through all properties of the object and validates each property key string against the subschema at `offset`. If any key fails validation, validation aborts. Non-object data nodes ignore this instruction.

---

### 3.Q OP_FORMAT (0x1B)
Asserts that a string instance conforms to a semantic format constraint.
```
+--------------------+---------------------------------+
|   OP_FORMAT(0x1B)  |       format (StringRef)        |
+--------------------+---------------------------------+
```
* **Format**: `[0x1B] [8 bytes format StringRef]` (9 bytes)
* **VM Semantics**: If the current data node is a string, the VM parses the expected format name in `format` (resolved from the Constant Pool) and validates the string contents against it.
  * Supported formats:
    * `"ipv4"`: 4 decimal octets (0-255) separated by dots, with no leading zeros.
    * `"email"`: Basic internet mail address checking (exactly one `@` with non-empty local and domain parts, and a dot in the domain).
    * `"uuid"`: 36 characters with standard `8-4-4-4-12` hex representation.
    * `"date-time"`: RFC 3339 date and time representation (with mandatory time zone offset or `Z`, correct month/day range, and leap year checks).
  * If validation fails, validation aborts with a `Jsonv_Format_error`. Non-string data nodes ignore this instruction.



