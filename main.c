#include "jsonv.h"
#include <stdio.h>
#include <stdlib.h>

char *read_file_to_buffer(const char *filename, size_t *out_size) {
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

void print_error(Jsonv_Context *ctx) {
  /*#region*/
  Jsonv_error_stack *errors = jsonv_ctx_errors(ctx);
  for (int i = 0; i < errors->count; i++)
    printf("Error %d: %s\n", i + 1, errors->messages[i]);
  /*#endregion*/
}

int main() {
  /*#region*/
  size_t size;
  int e = 0;
  char *json_schema = read_file_to_buffer("schema.json", &size);
  char *json_data = read_file_to_buffer("data.json", &size);
  Jsonv_Context *ctx = NULL;

  jsmntok_t *s_tok = NULL;
  e = jsonv_ctx_prepare_schema(&ctx, json_schema, &s_tok);
  if (e <= 0) {
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
  printf("\nres: %s\n", e ? "INVALID" : "VALID");
clean:
  jsonv_ctx_free(ctx);
  free(json_schema);
  free(json_data);
  return 0;
  /*#endregion*/
}
