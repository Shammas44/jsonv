#include "assert.h"
const Except Assert_Failed = { "Assertion failed", Jsonv_Assertion_Failed };
void (assert)(int e) {
	assert(e);
}
