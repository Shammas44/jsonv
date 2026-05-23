#ifndef _JSONV_PATH_H_INCLUDED
#define _JSONV_PATH_H_INCLUDED
#include "value.h"

Value value_get_path(Value current, const char *fmt, ...);

#endif
