#include "path.h"
#include "arr.h"
#include "obj.h"
#include <stdarg.h>

/* Retrieves a nested value using a path format string.
  Format characters:
    's' - Object Key (String)
    'i' - Array Index (Integer)

  Example usage:
  Value v = value_get_path(root, "ssi", "description", "prices", 1);

  Returns:
    - Target Value on success
    - val_unresolvable() if a valid property/index does not exist
    - val_impossible() if attempting a type mismatch (e.g., string key on an
  array)
*/
Value value_get_path(Value current, const char *fmt, ...) {
  /*#region*/
  va_list args;
  va_start(args, fmt);

  for (const char *p = fmt; *p != '\0'; p++) {
    // Stop early if the current value is already in an error state
    // Requires these enums to be defined in your value.h
    // Assuming VAL_OBJ and VAL_ARRAY are your only valid iterable tags
    if (current.tag != VAL_OBJ && current.tag != VAL_ARRAY) {
      current = val_impossible();
      break;
    }

    if (*p == 's') { // Object Key Lookup
      const char *key = va_arg(args, const char *);

      // Cannot use a string key on a non-object
      if (current.tag != VAL_OBJ) {
        current = val_impossible();
        break;
      }

      Obj *obj = (Obj *)current.as.p;
      Value next_val;

      // If the key doesn't exist, it's unresolvable
      if (!obj_get(obj, key, &next_val)) {
        current = val_unresolvable();
        break;
      }
      current = next_val;

    } else if (*p == 'i') { // Array Index Lookup
      int index = va_arg(args, int);

      // Cannot use an integer index on a non-array
      if (current.tag != VAL_ARRAY) {
        current = val_impossible();
        break;
      }

      Arr *arr = (Arr *)current.as.p;
      Value next_val;

      // If index is out of bounds, it's unresolvable
      if (!arr_get(arr, index, &next_val)) {
        current = val_unresolvable();
        break;
      }
      current = next_val;

    } else {
      // Invalid format character provided
      current = val_impossible();
      break;
    }
  }

  va_end(args);
  return current;
  /*#endregion*/
}
