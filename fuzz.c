#include "assert.h"
#include "jsonv.h"
#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include <fcntl.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>

/**
 * @brief Reads the entire contents of a file into a heap-allocated buffer.
 * @param filename The path to the file.
 * @param size Pointer to store the size of the data read.
 * @return Heap-allocated buffer containing the file contents, or NULL on error.
 */
static unsigned char *read_file(const char *filename, size_t *out_size) {
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

static bool jq_accepts(const unsigned char *buf, size_t len) {
  /*#region*/
  int inpipe[2];
  int pid;

  if (pipe(inpipe) != 0)
    return false;

  pid = fork();
  if (pid == 0) {
    /* child */
    dup2(inpipe[0], STDIN_FILENO);
    close(inpipe[0]);
    close(inpipe[1]);

    execlp("jq", "jq", "-e", "-s",
           "length == 1 and (.[0] | type == \"object\")", NULL);
    _exit(1);
  }

  /* parent */
  close(inpipe[0]);
  write(inpipe[1], buf, len);
  close(inpipe[1]);

  int status;
  waitpid(pid, &status, 0);

  return WIFEXITED(status) && WEXITSTATUS(status) == 0;
  /*#endregion*/
}

static bool starts_with_object(const unsigned char *buf, size_t len) {
  /*#region*/
  for (size_t i = 0; i < len; i++) {
    if (buf[i] == ' ' || buf[i] == '\n' || buf[i] == '\t' || buf[i] == '\r')
      continue;
    return buf[i] == '{';
  }
  return false;
  /*#endregion*/
}

static void dump_mismatch(const unsigned char *buf, size_t len, bool mine,
                          bool jq) {
  /*#region*/
  char path[256];

  mkdir("mismatches", 0755);

  snprintf(path, sizeof(path), "mismatches/mismatch_%d_%d_%d.json",
           getpid(), mine, jq);

  int fd = open(path, O_WRONLY | O_CREAT | O_EXCL, 0644);
  if (fd < 0)
    return;

  write(fd, buf, len);
  close(fd);
  /*#endregion*/
}

int main(int argc, char *argv[]) {
  /*#region*/
  if (argc != 2) {
    // AFL++ provides the filename as argv[1] (@@)
    fprintf(stderr, "Usage: %s <input_file>\n", argv[0]);
    return 1;
  }

  const char *input_filename = argv[1];
  size_t len = 0;
  unsigned char *json_data = NULL;

  json_data = read_file(input_filename, &len);
  assert(json_data);

  Jsonv_Arena *arena = jsonv_arena_new(4096, 1024 * 1024, 3 * 4096);
  assert(arena);
  bool jq;

  Jsonv_Config config = {
      .default_block_size = 1024,
      .max_limit = 65536,
      .shrink_at = 4096,
      .max_depth = 100,
      .max_values = 100,
      .max_objects = 100,
      .max_array = 100,
      .max_string_bytes = 1000
  };

  // Create the request-local context on the execution arena
  Jsonv_Arena_Error error = 0;
  Jsonv_Context *ctx = jsonv_ctx_new(arena, &config ,&error);
  if(!ctx && error > 0){
    return 0;
  }

  bool mine = jsonv_ctx_parse_data(ctx, json_data);

  if (!starts_with_object(json_data, len)) {
    jq = false;
  } else {
    jq = jq_accepts(json_data, len);
  }

  /* Coverage signal: valid JSON */
  if (mine) {
    volatile int valid = 1;
    (void)(valid);
  }

  /* Differential correctness check */
  if (mine != jq) {
    dump_mismatch(json_data, len, mine, jq);
  }

  free(json_data);
  jsonv_arena_destroy(arena);
  jsonv_free_all();
  return 0;
  /*#endregion*/
}

// for f in output_fuzz/default/crashes/id*; do
//     echo "Replaying $f"
//     ./bin/fuzz  "$f" || echo "Crash confirmed"
// done
