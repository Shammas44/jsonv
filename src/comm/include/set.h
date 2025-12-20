#ifndef _JSONV_SET_H_INCLUDED
#define _JSONV_SET_H_INCLUDED
#define T Set
typedef struct T *T;
T set_new(int hint, int cmp(const void *x, const void *y),
          unsigned hash(const void *x));
void set_free(T *set);
int set_length(T set);
int set_member(T set, const void *member);
void set_put(T set, const void *member);
void *set_remove(T set, const void *member);
void set_map(T set, void apply(const void *member, void *cl), void *cl);
void **set_to_array(T set, void *end);
T set_union(T s, T t);
T set_inter(T s, T t);
T set_minus(T s, T t);
T set_diff(T s, T t);
#undef T
#endif
