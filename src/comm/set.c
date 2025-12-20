#include <limits.h>
#include <stddef.h>
#include "mem.h"
#include "assert.h"
#include "arith.h"
#include "set.h"
#define T Set

struct T {
	int length;
	unsigned timestamp;
	int (*cmp)(const void *x, const void *y);
	unsigned (*hash)(const void *x);
	int size;
	struct member {
		struct member *link;
		const void *member;
	} **buckets;
};

static int cmpatom(const void *x, const void *y) {
/*#region*/
	return x != y;
/*#endregion*/
}

static unsigned hashatom(const void *x) {
/*#region*/
	return (unsigned long)x>>2;
/*#endregion*/
}

static T copy(T t, int hint) {
/*#region*/
	T set;
	assert(t);
	set = set_new(hint, t->cmp, t->hash);
	{ int i;
	  struct member *q;
	  for (i = 0; i < t->size; i++)
	  	for (q = t->buckets[i]; q; q = q->link)
		{
			struct member *p;
			const void *member = q->member;
			int i = (*set->hash)(member)%set->size;
			NEW(p);
			p->member = member;
			p->link = set->buckets[i];
			set->buckets[i] = p;
			set->length++;
		}
	}
	return set;
/*#endregion*/
}

T set_new(int hint,
	int cmp(const void *x, const void *y),
	unsigned hash(const void *x)) {
/*#region*/
	T set;
	int i;
	static int primes[] = { 509, 509, 1021, 2053, 4093,
		8191, 16381, 32771, 65521, INT_MAX };
	assert(hint >= 0);
	for (i = 1; primes[i] < hint; i++)
		;
	set = ALLOC(sizeof (*set) +
		primes[i-1]*sizeof (set->buckets[0]));
	set->size = primes[i-1];
	set->cmp  = cmp  ?  cmp : cmpatom;
	set->hash = hash ? hash : hashatom;
	set->buckets = (struct member **)(set + 1);
	for (i = 0; i < set->size; i++)
		set->buckets[i] = NULL;
	set->length = 0;
	set->timestamp = 0;
	return set;
/*#endregion*/
}

int set_member(T set, const void *member) {
/*#region*/
	int i;
	struct member *p;
	assert(set);
	assert(member);
	i = (*set->hash)(member)%set->size;
	for (p = set->buckets[i]; p; p = p->link)
		if ((*set->cmp)(member, p->member) == 0)
			break;
	return p != NULL;
/*#endregion*/
}

void set_put(T set, const void *member) {
/*#region*/
	int i;
	struct member *p;
	assert(set);
	assert(member);
	i = (*set->hash)(member)%set->size;
	for (p = set->buckets[i]; p; p = p->link)
		if ((*set->cmp)(member, p->member) == 0)
			break;
	if (p == NULL) {
		NEW(p);
		p->member = member;
		p->link = set->buckets[i];
		set->buckets[i] = p;
		set->length++;
	} else
		p->member = member;
	set->timestamp++;
/*#endregion*/
}

void *set_remove(T set, const void *member) {
/*#region*/
	int i;
	struct member **pp;
	assert(set);
	assert(member);
	set->timestamp++;
	i = (*set->hash)(member)%set->size;
	for (pp = &set->buckets[i]; *pp; pp = &(*pp)->link)
		if ((*set->cmp)(member, (*pp)->member) == 0) {
			struct member *p = *pp;
			*pp = p->link;
			member = p->member;
			FREE(p);
			set->length--;
			return (void *)member;
		}
	return NULL;
  /*#endregion*/
}

int set_length(T set) {
/*#region*/
	assert(set);
	return set->length;
/*#endregion*/
}

void set_free(T *set) {
/*#region*/
	assert(set && *set);
	if ((*set)->length > 0) {
		int i;
		struct member *p, *q;
		for (i = 0; i < (*set)->size; i++)
			for (p = (*set)->buckets[i]; p; p = q) {
				q = p->link;
				FREE(p);
			}
	}
	FREE(*set);
/*#endregion*/
}

void set_map(T set,
	void apply(const void *member, void *cl), void *cl) {
/*#region*/
	int i;
	unsigned stamp;
	struct member *p;
	assert(set);
	assert(apply);
	stamp = set->timestamp;
	for (i = 0; i < set->size; i++)
		for (p = set->buckets[i]; p; p = p->link) {
			apply(p->member, cl);
			assert(set->timestamp == stamp);
		}
/*#endregion*/
}

void **set_to_array(T set, void *end) {
/*#region*/
	int i, j = 0;
	void **array;
	struct member *p;
	assert(set);
	array = ALLOC((set->length + 1)*sizeof (*array));
	for (i = 0; i < set->size; i++)
		for (p = set->buckets[i]; p; p = p->link)
			array[j++] = (void *)p->member;
	array[j] = end;
	return array;
/*#endregion*/
}

T set_union(T s, T t) {
/*#region*/
	if (s == NULL) {
		assert(t);
		return copy(t, t->size);
	} else if (t == NULL)
		return copy(s, s->size);
	else {
		T set = copy(s, arith_max(s->size, t->size));
		assert(s->cmp == t->cmp && s->hash == t->hash);
		{ int i;
		  struct member *q;
		  for (i = 0; i < t->size; i++)
		  	for (q = t->buckets[i]; q; q = q->link)
			set_put(set, q->member);
		}
		return set;
	}
/*#endregion*/
}

T set_inter(T s, T t) {
/*#region*/
	if (s == NULL) {
		assert(t);
		return set_new(t->size, t->cmp, t->hash);
	} else if (t == NULL)
		return set_new(s->size, s->cmp, s->hash);
	else if (s->length < t->length)
		return set_inter(t, s);
	else {
		T set = set_new(arith_min(s->size, t->size),
			s->cmp, s->hash);
		assert(s->cmp == t->cmp && s->hash == t->hash);
		{ int i;
		  struct member *q;
		  for (i = 0; i < t->size; i++)
		  	for (q = t->buckets[i]; q; q = q->link)
			if (set_member(s, q->member))
				{
					struct member *p;
					const void *member = q->member;
					int i = (*set->hash)(member)%set->size;
					NEW(p);
					p->member = member;
					p->link = set->buckets[i];
					set->buckets[i] = p;
					set->length++;
				}
		}
		return set;
	}
/*#endregion*/
}

T set_minus(T t, T s) {
/*#region*/
	if (t == NULL){
		assert(s);
		return set_new(s->size, s->cmp, s->hash);
	} else if (s == NULL)
		return copy(t, t->size);
	else {
		T set = set_new(arith_min(s->size, t->size),
			s->cmp, s->hash);
		assert(s->cmp == t->cmp && s->hash == t->hash);
		{ int i;
		  struct member *q;
		  for (i = 0; i < t->size; i++)
		  	for (q = t->buckets[i]; q; q = q->link)
			if (!set_member(s, q->member))
				{
					struct member *p;
					const void *member = q->member;
					int i = (*set->hash)(member)%set->size;
					NEW(p);
					p->member = member;
					p->link = set->buckets[i];
					set->buckets[i] = p;
					set->length++;
				}
		}
		return set;
	}
/*#endregion*/
}

T set_diff(T s, T t) {
/*#region*/
	if (s == NULL) {
		assert(t);
		return copy(t, t->size);
	} else if (t == NULL)
		return copy(s, s->size);
	else {
		T set = set_new(arith_min(s->size, t->size),
			s->cmp, s->hash);
		assert(s->cmp == t->cmp && s->hash == t->hash);
		{ int i;
		  struct member *q;
		  for (i = 0; i < t->size; i++)
		  	for (q = t->buckets[i]; q; q = q->link)
			if (!set_member(s, q->member))
				{
					struct member *p;
					const void *member = q->member;
					int i = (*set->hash)(member)%set->size;
					NEW(p);
					p->member = member;
					p->link = set->buckets[i];
					set->buckets[i] = p;
					set->length++;
				}
		}
		{ T u = t; t = s; s = u; }
		{ int i;
		  struct member *q;
		  for (i = 0; i < t->size; i++)
		  	for (q = t->buckets[i]; q; q = q->link)
			if (!set_member(s, q->member))
				{
					struct member *p;
					const void *member = q->member;
					int i = (*set->hash)(member)%set->size;
					NEW(p);
					p->member = member;
					p->link = set->buckets[i];
					set->buckets[i] = p;
					set->length++;
				}
		}
		return set;
	}
/*#endregion*/
}
