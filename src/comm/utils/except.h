#ifndef _JSONV_EXCEPT_INCLUDED
#define _JSONV_EXCEPT_INCLUDED
#include <setjmp.h>

#define T Except

typedef enum {
  // number
  Jsonv_Maximum_error,
  Jsonv_Minimum_error,
  Jsonv_MultipleOf_error,
  Jsonv_ExclusiveMaximum_error,
  Jsonv_ExclusiveMinimum_error,
  // string
  Jsonv_MaxLength_error,
  Jsonv_MinLength_error,
  Jsonv_Pattern_error,
  // object
  Jsonv_Type_error,
  Jsonv_AdditionalProperties_error,
  Jsonv_MinProperties_error,
  Jsonv_MaxProperties_error,
  // array
  Jsonv_MinItems_error,
  Jsonv_Required_error,
  Jsonv_UniqueItems_error,
  Jsonv_Contains_error,
  Jsonv_Not_error,
  // else
  Jsonv_ValueNotAllowed_error,
  Jsonv_Malformed_json,
  Jsonv_Maximum_Token_Bytes_Reached,
  Jsonv_Maximum_Object_Reached,
  Jsonv_Maximum_Array_Reached,
  Jsonv_Maximum_Values_Reached,
  Jsonv_Mem_Failed,
  Jsonv_Arena_Limit_Reached,
  Jsonv_Maximum_Nested_Depth_Reached,
  Jsonv_Assertion_Failed,
  Jsonv_Compile_Regexp_Failed
} Jsonv_Except_Type;

typedef struct T {
	const char *reason;
  Jsonv_Except_Type type;
} T;

typedef struct Except_Frame Except_Frame;
struct Except_Frame {
	Except_Frame *prev;
	jmp_buf env;
	const char *file;
	int line;
	const T *exception;
};
enum { Except_entered=0, Except_raised,
       Except_handled,   Except_finalized };
extern Except_Frame *Except_stack;
extern const Except Assert_Failed;

void Except_raise(const T *e, const char *file,int line);
#ifdef WIN32
#include <windows.h>

extern int Except_index;
extern void Except_init(void);
extern void Except_push(Except_Frame *fp);
extern void Except_pop(void);
#endif
#ifdef WIN32
/* $Id$ */
#define RAISE(e) Except_raise(&(e), __FILE__, __LINE__)
#define RERAISE Except_raise(Except_frame.exception, \
	Except_frame.file, Except_frame.line)
#define RETURN switch (Except_pop(),0) default: return
#define TRY do { \
	volatile int Except_flag; \
	Except_Frame Except_frame; \
	if (Except_index == -1) \
		Except_init(); \
	Except_push(&Except_frame);  \
	Except_flag = setjmp(Except_frame.env); \
	if (Except_flag == Except_entered) {
#define EXCEPT(e) \
		if (Except_flag == Except_entered) Except_pop(); \
	} else if (Except_frame.exception == &(e)) { \
		Except_flag = Except_handled;
#define ELSE \
		if (Except_flag == Except_entered) Except_pop(); \
	} else { \
		Except_flag = Except_handled;
#define FINALLY \
		if (Except_flag == Except_entered) Except_pop(); \
	} { \
		if (Except_flag == Except_entered) \
			Except_flag = Except_finalized;
#define END_TRY \
		if (Except_flag == Except_entered) Except_pop(); \
		} if (Except_flag == Except_raised) RERAISE; \
} while (0)
#else
#define RAISE(e) Except_raise(&(e), __FILE__, __LINE__)
#define RERAISE Except_raise(Except_frame.exception, \
	Except_frame.file, Except_frame.line)
#define RETURN switch (Except_stack = Except_stack->prev,0) default: return
#define TRY do { \
	volatile int Except_flag; \
	Except_Frame Except_frame; \
	Except_frame.prev = Except_stack; \
	Except_stack = &Except_frame;  \
	Except_flag = setjmp(Except_frame.env); \
	if (Except_flag == Except_entered) {
#define EXCEPT(e) \
		if (Except_flag == Except_entered) Except_stack = Except_stack->prev; \
	} else if (Except_frame.exception == &(e)) { \
		Except_flag = Except_handled;
#define ELSE \
		if (Except_flag == Except_entered) Except_stack = Except_stack->prev; \
	} else { \
		Except_flag = Except_handled;
#define FINALLY \
		if (Except_flag == Except_entered) Except_stack = Except_stack->prev; \
	} { \
		if (Except_flag == Except_entered) \
			Except_flag = Except_finalized;
#define END_TRY \
		if (Except_flag == Except_entered) Except_stack = Except_stack->prev; \
		} if (Except_flag == Except_raised) RERAISE; \
} while (0)
#endif
#undef T
#endif
