#ifndef _JSONV_NUMBER_INCLUDED
#define _JSONV_NUMBER_INCLUDED
#include <stdbool.h>

bool parse_int(const char *s, int *out);

bool parse_double(const char *s, double *out);
#endif
