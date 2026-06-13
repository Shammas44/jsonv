#ifndef _JSONV_ATOM_H
#define _JSONV_ATOM_H
      int   atom_length(const char *str);
const char *atom_new   (const char *str, int len);
const char *atom_string(const char *str);
const char *atom_int   (long n);
void        atom_clear (void);
#endif

