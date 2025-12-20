#ifndef _JSONV_ATOM_H_INCLUDED
#define _JSONV_ATOM_H_INCLUDED
      int   atom_length(const char *str);
const char *atom_new   (const char *str, int len);
const char *atom_string(const char *str);
const char *atom_int   (long n);
#endif
