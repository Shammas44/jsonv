#include "jsonv.h"
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define KB(x) 1024 * x
#define MB(x) 1024 * 1024 * x

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

static inline uint64_t now_ns(void) {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (uint64_t)ts.tv_sec * 1000000000ull + ts.tv_nsec;
}

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

#define MAX_PATH_LENGTH 1024
#define INITIAL_CAPACITY 16

bool file_path_list_add(FilePathList *list, const char *path) {
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

void list_files_recursive_helper(const char *basePath, FilePathList *list) {
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

bool file_path_list_init(FilePathList *list) {
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

void file_path_list_free(FilePathList *list) {
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

FilePathList file_list_recursively(const char *basePath) {
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

unsigned char *file_read(const char *filename, size_t *out_size) {
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
  unsigned char *buffer = malloc(size + 1);
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

void single_payload(unsigned char *payload, unsigned char *schema_json,
                    Jsonv_Arena *arena) {
  /*#region*/
  // Allocate a separate arena for the read-only schema compile phase
  Jsonv_Arena *schema_arena = jsonv_arena_new(4096, 1024 * 1024, 12 * 1024);
  if (!schema_arena) {
    event_log(Red, "Error: Failed to create schema arena");
    return;
  }

  Jsonv_Config config = {.default_block_size = 1024,
                         .max_limit = 65536,
                         .shrink_at = 4096,
                         .max_depth = 10,
                         .max_values = 100,
                         .max_objects = 100,
                         .max_array = 100,
                         .max_string_bytes = 1000};

  Jsonv_Error err = {0};
  Jsonv_Schema *schema = NULL;
  if (schema_json) {
    schema = jsonv_schema_compile(schema_arena, schema_json, &config, &err);
  }

  Jsonv_Arena_Error error = 0;
  Jsonv_Context *ctx = jsonv_ctx_new(arena, &config, &error);
  if (!ctx) {
    event_log(Red, "Error: Failed to create context");
    jsonv_arena_destroy(schema_arena);
    return;
  }

  bool parsed = jsonv_ctx_parse_data(ctx, payload);
  if (parsed) {
    event_log(Green, "Success: Payload parsed successfully.");
    bool valid = true;
    if (schema) {
      valid = jsonv_ctx_validate(ctx, schema);
      if (valid) {
        event_log(Green, "Success: Payload is valid against the schema.");
      } else {
        const Jsonv_Error *v_err = jsonv_ctx_get_error(ctx);
        event_log(Red, "Validation Error %d: %s at %s", v_err->type,
                  v_err->description, v_err->path ? v_err->path : "");
      }
    }
    if (valid) {
      Jsonv_Value parsed_val;
      if (jsonv_ctx_get_value(ctx, &parsed_val)) {
        // Successfully retrieved shape-based Value on-demand.
      }
    }
  } else {
    const Jsonv_Error *p_err = jsonv_ctx_get_error(ctx);
    event_log(Red, "Parse Error %d: %s at %s", p_err->type, p_err->description,
              p_err->path ? p_err->path : "");
  }

  // Deallocate schema arena to prevent memory leaks
  jsonv_arena_destroy(schema_arena);
  /*#endregion*/
}

void single_file(char *path, unsigned char *schema, Jsonv_Arena *arena) {
  /*#region*/
  size_t size;
  unsigned char *json_data = file_read(path, &size);
  if (!json_data) {
    event_log(Red, "Error: Failed to read file %s", path);
    return;
  }
  printf("input: %.250s\n", json_data);
  single_payload(json_data, schema, arena);
  free(json_data);
  /*#endregion*/
}

void multiple_files(char *path, unsigned char *schema, Jsonv_Arena *arena) {
  /*#region*/
  FilePathList files = file_list_recursively(path);
  printf("Found %zu files\n", files.count);
  for (size_t i = 0; i < files.count; i++) {
    printf("=== CASE %zu ==============\n", i);
    printf("%s\n", files.paths[i]);
    single_file(files.paths[i], schema, arena);
  }
  file_path_list_free(&files);
  /*#endregion*/
}

int main() {
  /*#region*/
  Jsonv_Arena *arena = jsonv_arena_new(KB(4), MB(1), KB(12));
  uint64_t start = now_ns();
  // for (int i = 0; i < 1000; i++) {
  // Default setup: 4KB blocks, 1MB limit, 12KB trim threshold
  size_t size;
  unsigned char *schema = file_read("./schema2.json", &size);

  char data[] = "{"
                "\"name\": \"iphone4\","
                "\"price\": 2,"
                "\"price\": 5,"
                "\"price\": 7,"
                "\"description\": {"
                "   \"forbidden\": \"test\","
                "   \"forbidden\": \"yo\","
                "   \"name\": \"test\","
                "   \"prices\": ["
                "       4, 6"
                "     ]"
                "   }"
                "}";
  single_payload((unsigned char *)data, schema, arena);
  // single_file("./seed_corpus/valid2.json", schema, arena);
  // multiple_files("./mismatches", schema, arena);
  // }

  uint64_t end = now_ns();
  uint64_t elapsed_ns = end - start;
  uint64_t seconds = elapsed_ns / 1000000000ull;
  uint64_t milliseconds = (elapsed_ns % 1000000000ull) / 1000000ull;
  printf("time: %llu.%03llu s\n", (unsigned long long)seconds,
         (unsigned long long)milliseconds);

  jsonv_arena_destroy(arena);
  free(schema);
  jsonv_free_all();
  return 0;
  /*#endregion*/
}
