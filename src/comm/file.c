#include "file.h"
#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void list_files_recursive_helper(const char *basePath, FilePathList *list) {
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
