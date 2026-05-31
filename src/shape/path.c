#include "shape.internal.h"
#include <stdarg.h>

/* Retrieves a nested value using a path format string. */
Value value_get_path(Value current, const char *fmt, ...) {
  /*#region*/
  va_list args;
  va_start(args, fmt);

  for (const char *p = fmt; *p != '\0'; p++) {
    if (current.tag != VAL_OBJ && current.tag != VAL_ARRAY) {
      current = val_impossible();
      break;
    }

    if (*p == 's') { // Object Key Lookup
      const char *key = va_arg(args, const char *);

      if (current.tag != VAL_OBJ) {
        current = val_impossible();
        break;
      }

      Obj *obj = (Obj *)current.as.p;
      Value next_val;

      if (!obj_get(obj, key, &next_val)) {
        current = val_unresolvable();
        break;
      }
      current = next_val;

    } else if (*p == 'i') { // Array Index Lookup
      int index = va_arg(args, int);

      if (current.tag != VAL_ARRAY) {
        current = val_impossible();
        break;
      }

      Arr *arr = (Arr *)current.as.p;
      Value next_val;

      if (!arr_get(arr, index, &next_val)) {
        current = val_unresolvable();
        break;
      }
      current = next_val;

    } else {
      current = val_impossible();
      break;
    }
  }

  va_end(args);
  return current;
  /*#endregion*/
}

Jsonv_Value jsonv_value_get_path(Jsonv_Value current, const char *fmt, ...) {
  /*#region*/
  va_list args;
  va_start(args, fmt);

  for (const char *p = fmt; *p != '\0'; p++) {
    if (current.tag != JSONV_VAL_OBJ && current.tag != JSONV_VAL_ARRAY) {
      current = val_impossible();
      break;
    }

    if (*p == 's') {
      const char *key = va_arg(args, const char *);

      if (current.tag != JSONV_VAL_OBJ) {
        current = val_impossible();
        break;
      }

      Obj *obj = (Obj *)current.as.p;
      Value next_val;

      if (!obj_get(obj, key, &next_val)) {
        current = val_unresolvable();
        break;
      }
      current = next_val;

    } else if (*p == 'i') {
      int index = va_arg(args, int);

      if (current.tag != JSONV_VAL_ARRAY) {
        current = val_impossible();
        break;
      }

      Arr *arr = (Arr *)current.as.p;
      Value next_val;

      if (!arr_get(arr, index, &next_val)) {
        current = val_unresolvable();
        break;
      }
      current = next_val;

    } else {
      current = val_impossible();
      break;
    }
  }

  va_end(args);
  return current;
  /*#endregion*/
}
