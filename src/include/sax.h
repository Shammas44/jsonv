#ifndef _JSONV_SAX_H
#define _JSONV_SAX_H

#include <stdbool.h>
#include <stddef.h>
#include "macro.h"

typedef struct {
  bool (*on_begin_object)(void *user_data);
  bool (*on_end_object)(void *user_data);
  bool (*on_begin_array)(void *user_data);
  bool (*on_end_array)(void *user_data);
  bool (*on_object_key)(const unsigned char *key, size_t key_len, void *user_data);
  bool (*on_string)(const unsigned char *val, size_t val_len, void *user_data);
  bool (*on_number)(const unsigned char *val, size_t val_len, void *user_data);
  bool (*on_boolean)(bool val, void *user_data);
  bool (*on_null)(void *user_data);
} jsonv_sax_callbacks;

JSONV_API bool jsonv_parse_sax(
    const unsigned char *buffer,
    size_t length,
    const jsonv_sax_callbacks *callbacks,
    void *user_data
);

#endif
