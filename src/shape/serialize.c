#include "shape.internal.h"
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <math.h>

typedef struct Visited {
  const void *ptr;
  const struct Visited *next;
} Visited;

typedef struct {
  char *buf;
  size_t buf_sz;
  size_t written;
  bool error;
} JsonWriter;

static int visited_contains(const Visited *v, const void *ptr) {
  /*#region*/
  while (v) {
    if (v->ptr == ptr)
      return 1;
    v = v->next;
  }
  return 0;
  /*#endregion*/
}

static void write_char(JsonWriter *w, char c) {
  /*#region*/
  if (w->error) return;
  if (w->buf && w->written < w->buf_sz - 1) {
    w->buf[w->written] = c;
  }
  w->written++;
  /*#endregion*/
}

static void write_str(JsonWriter *w, const char *s, size_t len) {
  /*#region*/
  if (w->error) return;
  for (size_t i = 0; i < len; i++) {
    write_char(w, s[i]);
  }
  /*#endregion*/
}

static void write_escaped_str(JsonWriter *w, const char *s, size_t len) {
  /*#region*/
  write_char(w, '"');
  for (size_t i = 0; i < len; i++) {
    unsigned char c = (unsigned char)s[i];
    switch (c) {
      case '"':
        write_str(w, "\\\"", 2);
        break;
      case '\\':
        write_str(w, "\\\\", 2);
        break;
      case '\n':
        write_str(w, "\\n", 2);
        break;
      case '\r':
        write_str(w, "\\r", 2);
        break;
      case '\t':
        write_str(w, "\\t", 2);
        break;
      case '\b':
        write_str(w, "\\b", 2);
        break;
      case '\f':
        write_str(w, "\\f", 2);
        break;
      default:
        if (c < 0x20) {
          char hex_buf[7];
          int hex_len = snprintf(hex_buf, sizeof(hex_buf), "\\u%04x", c);
          write_str(w, hex_buf, (size_t)hex_len);
        } else {
          write_char(w, (char)c);
        }
        break;
    }
  }
  write_char(w, '"');
  /*#endregion*/
}

static void serialize_val(JsonWriter *w, Value v, const Visited *visited) {
  /*#region*/
  if (w->error) return;

  switch (v.tag) {
    case VAL_NULL:
      write_str(w, "null", 4);
      break;

    case VAL_BOOLEAN:
      if (v.as.boolean) {
        write_str(w, "true", 4);
      } else {
        write_str(w, "false", 5);
      }
      break;

    case VAL_INT: {
      char num_buf[32];
      int num_len = snprintf(num_buf, sizeof(num_buf), "%lld", (long long)v.as.i);
      write_str(w, num_buf, (size_t)num_len);
      break;
    }

    case VAL_DOUBLE: {
      if (isnan(v.as.d) || isinf(v.as.d)) {
        w->error = true;
      } else {
        char num_buf[64];
        int num_len = snprintf(num_buf, sizeof(num_buf), "%.17g", v.as.d);
        write_str(w, num_buf, (size_t)num_len);
      }
      break;
    }

    case VAL_STRING: {
      size_t len = val_str_len(v);
      write_escaped_str(w, (const char *)v.as.p, len);
      break;
    }

    case VAL_OBJ: {
      Obj *o = (Obj *)v.as.p;
      if (!o) {
        write_str(w, "null", 4);
        break;
      }
      if (visited_contains(visited, o)) {
        w->error = true;
        return;
      }

      Visited local_visited;
      local_visited.ptr = o;
      local_visited.next = visited;

      write_char(w, '{');
      int first = 1;
      for (int i = 0; i < o->shape->slot_count; i++) {
        Value val = o->slots[i];
        if (val.tag == VAL_UNDEFINED || val.tag == VAL_UNRESOLVABLE || val.tag == VAL_IMPOSSIBLE) {
          continue;
        }
        const char *key = shape_get_key_at(o->shape, i);
        if (!first) {
          write_char(w, ',');
        }
        first = 0;
        write_escaped_str(w, key, strlen(key));
        write_char(w, ':');
        serialize_val(w, val, &local_visited);
      }
      write_char(w, '}');
      break;
    }

    case VAL_ARRAY: {
      Arr *a = (Arr *)v.as.p;
      if (!a) {
        write_str(w, "null", 4);
        break;
      }
      if (visited_contains(visited, a)) {
        w->error = true;
        return;
      }

      Visited local_visited;
      local_visited.ptr = a;
      local_visited.next = visited;

      write_char(w, '[');
      for (int i = 0; i < a->length; i++) {
        if (i > 0) {
          write_char(w, ',');
        }
        Value val = a->items[i];
        if (val.tag == VAL_UNDEFINED || val.tag == VAL_UNRESOLVABLE || val.tag == VAL_IMPOSSIBLE) {
          write_str(w, "null", 4);
        } else {
          serialize_val(w, val, &local_visited);
        }
      }
      write_char(w, ']');
      break;
    }

    case VAL_UNDEFINED:
    case VAL_UNRESOLVABLE:
    case VAL_IMPOSSIBLE:
    default:
      w->error = true;
      break;
  }
  /*#endregion*/
}

int jsonv_serialize(Jsonv_Value v, char *buf, size_t buf_sz) {
  /*#region*/
  JsonWriter w;
  w.buf = buf;
  w.buf_sz = buf_sz;
  w.written = 0;
  w.error = false;

  serialize_val(&w, v, NULL);

  if (w.error) {
    return -1;
  }

  if (w.buf && w.buf_sz > 0) {
    size_t null_idx = w.written < w.buf_sz ? w.written : w.buf_sz - 1;
    w.buf[null_idx] = '\0';
  }

  return (int)w.written;
  /*#endregion*/
}
