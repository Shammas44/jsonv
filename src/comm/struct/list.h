#ifndef _JSONV_LIST_H
#define _JSONV_LIST_H
#define T List
typedef struct T *T;
struct T {
	T rest;
	void *first;
};

T      list_new     (void *x, ...);
T      list_append  (T list, T tail);
T      list_copy    (T list);
T      list_pop     (T list, void **x);
T      list_push    (T list, void *x);
T      list_reverse (T list);
int    list_length  (T list);
void   list_free    (T *list);
void   list_map     (T list, void apply(void **x, void *cl), void *cl);
void **list_to_array(T list, void *end);
#undef T
#endif
