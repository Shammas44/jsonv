#ifndef _JSONV_DATA_H_INCLUDED
#define _JSONV_DATA_H_INCLUDED
#include "hint.h"
#include "type.h"
#include <jsmn/jsmn.h>
#include <sys/_types/_size_t.h>

typedef struct Jsonv_DataNode Jsonv_DataNode;

typedef struct Jsonv_DataNode {
  jsonv_t type;
  Jsonv_DataNode *parent;
  jsonv_Hint key;
  jsonv_Hint value;
  Jsonv_DataNode **properties;
  size_t property_count;
} Jsonv_DataNode;

void jsonv_data_free(Jsonv_DataNode *node);

char *jsonv_build_data_path(const Jsonv_DataNode *node, const char *json);
#endif
