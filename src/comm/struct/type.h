#ifndef _JSONV_TYPE_H_INCLUDED
#define _JSONV_TYPE_H_INCLUDED

typedef enum {
  jsonv_STRING,
  jsonv_NUMBER,
  jsonv_INTEGER,
  jsonv_BOOLEAN,
  jsonv_OBJECT,
  jsonv_ARRAY,
  jsonv_NULL,
  jsonv_UNKNOWN
} jsonv_t;

#endif
