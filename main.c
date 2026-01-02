#include "jsmn.h"
#include "jsonv.h"
#include "lexer.h"
#include <dirent.h>
#include <errno.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
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

#define MAX_PATH_LENGTH 1024
#define INITIAL_CAPACITY 16
#define KEY(index)                                                             \
  ({                                                                           \
    char *_key = colors[index];                                                \
    _key;                                                                      \
  })

typedef struct {
  char **paths;
  size_t count;
  size_t capacity;
} FilePathList;

static bool file_path_list_init(FilePathList *list) {
  /*#region*/
  list->paths = (char **)malloc(INITIAL_CAPACITY * sizeof(char *));
  if (list->paths == NULL) {
    list->count = 0;
    list->capacity = 0;
    return false;
  }
  list->count = 0;
  list->capacity = INITIAL_CAPACITY;
  return true;
  /*#endregion*/
}

static void file_path_list_free(FilePathList *list) {
  /*#region*/
  if (!list)
    return;
  for (size_t i = 0; i < list->count; i++) {
    free(list->paths[i]); // Free the path string itself
  }
  free(list->paths); // Free the array of pointers
  list->paths = NULL;
  list->count = 0;
  list->capacity = 0;
  /*#endregion*/
}

static bool file_path_list_add(FilePathList *list, const char *path) {
  /*#region*/
  if (list->count == list->capacity) {
    size_t new_capacity = list->capacity * 2;
    char **new_paths =
        (char **)realloc(list->paths, new_capacity * sizeof(char *));

    if (new_paths == NULL) {
      return false; // Reallocation failed
    }
    list->paths = new_paths;
    list->capacity = new_capacity;
  }

  // Duplicate the string and store the heap pointer
  list->paths[list->count] = strdup(path);
  if (list->paths[list->count] == NULL) {
    return false; // strdup failed
  }
  list->count++;
  return true;
  /*#endregion*/
}

static void list_files_recursive_helper(const char *basePath,
                                        FilePathList *list) {
  /*#region*/
  char path[MAX_PATH_LENGTH];
  struct dirent *dp;
  DIR *dir = NULL;

  dir = opendir(basePath);
  if (!dir) {
    // Log the error but continue execution
    fprintf(stderr, "Warning: Could not open directory %s: %s\n", basePath,
            strerror(errno));
    return;
  }

  while ((dp = readdir(dir)) != NULL) {
    const char *filename = dp->d_name;

    // Skip current (.) and parent (..) directories
    if (strcmp(filename, ".") == 0 || strcmp(filename, "..") == 0) {
      continue;
    }

    // Construct the full path
    if (snprintf(path, sizeof(path), "%s/%s", basePath, filename) >=
        (int)sizeof(path)) {
      // Path buffer overflow (handle gracefully)
      fprintf(stderr, "Warning: Path exceeded MAX_PATH_LENGTH: %s/%s\n",
              basePath, filename);
      continue;
    }

    if (dp->d_type == DT_DIR) {
      // Recurse into subdirectory
      list_files_recursive_helper(path, list);

    } else if (dp->d_type == DT_REG) {
      // --- NEW: Check for .txt extension ---
      size_t name_len = strlen(filename);

      // Check if the filename is long enough to contain ".txt" (at least 4
      // characters)
      if (name_len >= 4 && strcmp(filename + name_len - 4, ".txt") == 0) {
        // Ignore the file if it ends in ".txt"
        continue;
      }
      // --- END NEW CHECK ---

      // Add regular file path to the dynamic array
      if (!file_path_list_add(list, path)) {
        fprintf(stderr, "Error: Failed to allocate memory for path: %s\n",
                path);
        // Exit or handle memory allocation failure
      }
    }
  }

  closedir(dir);
  /*#endregion*/
}

static FilePathList list_files_recursively(const char *basePath) {
  FilePathList list;
  /*#region*/
  if (!file_path_list_init(&list)) {
    fprintf(stderr, "Fatal Error: Failed to initialize file path list.\n");
    // Return an empty, initialized list on failure
    list.paths = NULL;
    list.count = 0;
    list.capacity = 0;
    return list;
  }

  list_files_recursive_helper(basePath, &list);
  return list;
  /*#endregion*/
}

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

static void event_log(Keys key, const char *format, ...) {
  /*#region*/
  time_t t;
  time(&t);
  static char message_buffer[100 + 128];
  va_list args;
  va_start(args, format);
  vsnprintf(message_buffer, sizeof(message_buffer), format, args);
  va_end(args);
  char *start = KEY(key);
  char *end = KEY(Reset);
  char *date = timestamp_to_string(t);
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
  for (size_t i = 0; i < errors->count; i++) {
    event_log(Yellow, "Error %d: %s", i + 1, errors->messages[i]);
  }
  /*#endregion*/
}

static int logic(char *data, char *schema, Jsonv_Context **ctx) {
  /*#region*/
  (void)(schema);
  int e = 0;
  // char *json_schema = read_file("schema.json", &size);
  // char *json_data = read_file("data.json", &size);

  // jsmntok_t *s_tok = NULL;
  // e = jsonv_ctx_prepare_schema(&ctx, json_schema, &s_tok);
  // if (e < 0) {
  //   print_error(ctx);
  //   goto clean;
  // }
  // jsonv_ctx_print_schema(ctx, json_schema);

  e = jsonv_ctx_prepare_data(ctx, data);
  if (e <= 0) {
    goto clean;
  }
  // jsonv_ctx_print_data(ctx, data);

  // e = jsonv_ctx_validate(ctx, json_schema, json_data);
  // if (e) {
  //   print_error(ctx);
  //   goto clean;
  // }
clean:
  // free(json_schema);
  return e;
  /*#endregion*/
}

void single_file(char *path) {
  /*#region*/
  Jsonv_Context *ctx = NULL;
  size_t size;
  char *json_data = read_file(path, &size);
  printf("input: %s\n", json_data);
  int e = logic(json_data, NULL, &ctx);
  if (e > 0) {
    event_log(Green, "Succes: %s", "Payload parsed.");
  } else {
    print_error(ctx);
  }
  free(json_data);
  if (ctx)
    jsonv_ctx_free(ctx);
  /*#endregion*/
}

void single_payload(char *payload) {
  /*#region*/
  Jsonv_Context *ctx = NULL;
  printf("input: %s\n", payload);
  int e = logic(payload, NULL, &ctx);
  if (e > 0) {
    event_log(Green, "Succes: %s", "Payload parsed.");
    jsonv_ctx_print_data(ctx, payload);
  } else {
    print_error(ctx);
  }
  if (ctx)
    jsonv_ctx_free(ctx);
  /*#endregion*/
}

int main() {
  /*#region*/
  single_payload("{\"key\":\"value1\", \"key2\": [\"item1\", \"item2\", {\"key\": 123}]}");
  // single_file("./seed_corpus/valid2.json");
  return 0;
  const char *start_dir = "./output_fuzz/default/queue";
  FilePathList files = list_files_recursively(start_dir);

  printf("Found %zu files\n", files.count);
  for (size_t i = 0; i < files.count; i++) {
    Jsonv_Context *ctx = NULL;

    size_t size;
    char *json_data = read_file(files.paths[i], &size);
    int e = logic(json_data, NULL, &ctx);
    if (e > 0) {
      printf("=== CASE %zu ==============\n", i);
      printf("%s\n", files.paths[i]);
      printf("input: %s\n", json_data);
      event_log(Green, "Succes 1: %s", "$ Payload parsed.");
    } else {
      // print_error(ctx);
    }
    free(json_data);
    if (ctx)
      jsonv_ctx_free(ctx);
  }

  file_path_list_free(&files);
  /*#endregion*/
}
