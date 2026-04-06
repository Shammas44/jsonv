#include "file.h"
#include "jsonv.h"
#include "shape.h"
#include "global.h"
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

Shape *_g_root = NULL;

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

// static bool jq_accepts(const char *buf, size_t len) {
//   /*#region*/
//   int inpipe[2];
//   int pid;

//   if (pipe(inpipe) != 0)
//     return false;

//   pid = fork();
//   if (pid == 0) {
//     /* child */
//     dup2(inpipe[0], STDIN_FILENO);
//     close(inpipe[0]);
//     close(inpipe[1]);

//     execlp("jq", "jq", "-e", "-s",
//            "length == 1 and (.[0] | type == \"object\")", NULL);
//     _exit(1);
//   }

//   /* parent */
//   close(inpipe[0]);
//   ssize_t k = write(inpipe[1], buf, len);
//   (void)(k);
//   close(inpipe[1]);

//   int status;
//   waitpid(pid, &status, 0);

//   return WIFEXITED(status) && WEXITSTATUS(status) == 0;
//   /*#endregion*/
// }

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

static void print_error(Jsonv_Context *ctx) {
  /*#region*/
  E *error = jsonv_ctx_error(ctx);
  char *path = error->path ? error->path : "";
  event_log(Yellow, "Error %d: %s %s", error->type, error->description, path);
  /*#endregion*/
}

static bool logic(unsigned char *data, unsigned char *schema,
                  Jsonv_Context **ctx, Arena *arena) {
  /*#region*/
  (void)(schema);
  bool e = jsonv_ctx_init(ctx, arena, NULL);
  // if (e)
  //   e = jsonv_ctx_prepare_schema(ctx, schema);
  if (e)
    e = jsonv_ctx_prepare_data(ctx, data);
  // if (e)
  //   e = jsonv_ctx_validate(*ctx);
  return e;
  /*#endregion*/
}

void single_file(char *path, unsigned char *schema, Arena *arena) {
  /*#region*/
  Jsonv_Context *ctx = NULL;
  size_t size;
  unsigned char *json_data = file_read(path, &size);
  printf("input: %s\n", json_data);
  bool e = logic(json_data, schema, &ctx, arena);
  if (e) {
    event_log(Green, "Succes: %s", "Payload parsed.");
  } else {
    print_error(ctx);
  }
  free(json_data);
  if (ctx)
    jsonv_ctx_free(ctx);
  /*#endregion*/
}

void multiple_files(char *path, unsigned char *schema, Arena *arena) {
  /*#region*/
  (void)(schema);
  FilePathList files = file_list_recursively(path);
  printf("Found %zu files\n", files.count);
  for (size_t i = 0; i < files.count; i++) {
    Jsonv_Context *ctx = NULL;

    size_t size;
    unsigned char *json_data = file_read(files.paths[i], &size);
    bool e = logic(json_data, NULL, &ctx, arena);
    printf("=== CASE %zu ==============\n", i);
    printf("%s\n", files.paths[i]);
    printf("input: %s\n", json_data);
    if (e) {
      event_log(Green, "Succes 1: %s", "$ Payload parsed.");
    } else {
      event_log(Red, "Error 1: %s", "$ Payload unvalid.");
      // print_error(ctx);
    }
    free(json_data);
    if (ctx)
      jsonv_ctx_free(ctx);
  }

  file_path_list_free(&files);
  /*#endregion*/
}

void single_payload(unsigned char *payload, unsigned char *schema,
                    Arena *arena) {
  /*#region*/
  Jsonv_Context *ctx = NULL;
  // printf("input: %s\n", payload);
  bool e = logic(payload, schema, &ctx, arena);
  (void)(e);
  if (e) {
    event_log(Green, "Succes: %s", "Payload parsed.");
    jsonv_ctx_print_data(ctx);
    jsonv_ctx_print_schema(ctx);
  } else {
    print_error(ctx);
  }
  if (ctx)
    jsonv_ctx_free(ctx);
  /*#endregion*/
}

int main() {
  /*#region*/
  _g_root = shape_root();
  uint64_t start = now_ns();
  // for (int i = 0; i < 1000; i++) {
  // Default setup: 4KB blocks, 1MB limit, 12KB trim threshold
  Arena *arena = arena_new(KB(4), MB(1), KB(12));
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
  // multiple_files("./output_fuzz/default/crashes", schema, arena);
  arena_destroy(arena);
  free(schema);
  // }

  uint64_t end = now_ns();
  uint64_t elapsed_ns = end - start;
  uint64_t seconds = elapsed_ns / 1000000000ull;
  uint64_t milliseconds = (elapsed_ns % 1000000000ull) / 1000000ull;
  printf("time: %llu.%03llu s\n", (unsigned long long)seconds,
         (unsigned long long)milliseconds);
  return 0;
  /*#endregion*/
}
