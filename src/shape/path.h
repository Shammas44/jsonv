#ifndef _JSONV_PATH_H
#define _JSONV_PATH_H
#include "value.h"

Value value_get_path(Value current, const char *fmt, ...);

#endif
