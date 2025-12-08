#include "assert.h"
const Except Assert_Failed = { "Assertion failed" };
void (assert)(int e) {
	assert(e);
}
