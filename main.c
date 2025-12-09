#include "jsonv.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

static char *colors[] = {
    "\x1b[30m", "\x1b[31m", "\x1b[32m", "\x1b[33m", "\x1b[34m",
    "\x1b[35m", "\x1b[36m", "\x1b[37m", "\x1b[0m",
};

typedef enum {
  Black,
  Red,
  Green,
  Yellow,
  Blue,
  Magenta,
  Cyan,
  White,
  Reset,
} Keys;

#define KEY(index)                                                             \
  ({                                                                           \
    char *_key = colors[index];                                                \
    _key;                                                                      \
  })

static char *timestamp_to_string(size_t timestamp) {
  /*#region*/
  struct tm *local_time;
  static char str[20]; // "YYYY-MM-DD HH:MM:SS" + '\0'
  local_time = localtime((const time_t *)&timestamp);
  // Format it as: "YYYY-MM-DD HH:MM:SS"
  strftime(str, sizeof(str), "%H:%M:%S", local_time);
  return str;
  /*#endregion*/
}

static void event_log(const char *format, ...) {
  /*#region*/
  time_t t;
  time(&t);

  // --- Message Buffers ---
  // This buffer holds the formatted user message (e.g., "User %s logged in").
  static char message_buffer[100 + 128];

  // --- 1. Format the Variadic Message ---
  va_list args;
  va_start(args, format);
  // Vsnprintf safely writes the formatted string to message_buffer, preventing
  // overflow.
  vsnprintf(message_buffer, sizeof(message_buffer), format, args);
  va_end(args);

  // --- 2. Assemble Final Log Line ---
  char *start = KEY(Yellow);
  char *end = KEY(Reset);
  char *date = timestamp_to_string(t);

  // --- 3. Output to Console ---
  printf("[%s%s%s] %s%s%s\n", KEY(Magenta), date, KEY(Reset), start,
         message_buffer, end);
  /*#endregion*/
}

static char *read_file(const char *filename, size_t *out_size) {
  /*#region*/
  FILE *fp = fopen(filename, "rb");
  if (!fp)
    return NULL;

  // Move to end to determine file size
  if (fseek(fp, 0, SEEK_END) != 0) {
    fclose(fp);
    return NULL;
  }

  long size = ftell(fp);
  if (size < 0) {
    fclose(fp);
    return NULL;
  }
  rewind(fp);

  // Allocate buffer (+1 for NULL terminator)
  char *buffer = malloc(size + 1);
  if (!buffer) {
    fclose(fp);
    return NULL;
  }

  // Read file into buffer
  size_t read_bytes = fread(buffer, 1, size, fp);
  fclose(fp);

  if (read_bytes != (size_t)size) {
    free(buffer);
    return NULL;
  }

  buffer[size] = '\0'; // Null terminate
  if (out_size)
    *out_size = size;

  return buffer;
  /*#endregion*/
}

static void print_error(Jsonv_Context *ctx) {
  /*#region*/
  Jsonv_error_stack *errors = jsonv_ctx_errors(ctx);
  puts("");
  for (int i = 0; i < errors->count; i++) {
    event_log("Error %d: %s", i + 1, errors->messages[i]);
  }

  /*#endregion*/
}

int main() {
  /*#region*/
  size_t size;
  int e = 0;
  char *json_schema = read_file("schema.json", &size);
  char *json_data = read_file("data.json", &size);
  Jsonv_Context *ctx = NULL;

  jsmntok_t *s_tok = NULL;
  e = jsonv_ctx_prepare_schema(&ctx, json_schema, &s_tok);
  if (e < 0) {
    print_error(ctx);
    goto clean;
  }
  jsonv_ctx_print_schema(ctx, json_schema);

  jsmntok_t *d_tok = NULL;
  e = jsonv_ctx_prepare_data(&ctx, json_data, &d_tok);
  if (e <= 0) {
    print_error(ctx);
    goto clean;
  }
  jsonv_ctx_print_data(ctx, json_data);

  e = jsonv_ctx_validate(ctx, json_schema, json_data);
  if (e) {
    print_error(ctx);
    goto clean;
  }
clean:
  jsonv_ctx_free(ctx);
  free(json_schema);
  free(json_data);
  return 0;
  /*#endregion*/
}
