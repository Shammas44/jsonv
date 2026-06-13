#ifndef _JSONV_FILE_H
#define _JSONV_FILE_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
  char **paths;
  size_t count;
  size_t capacity;
} FilePathList;

#define MAX_PATH_LENGTH 1024
#define INITIAL_CAPACITY 16

bool file_path_list_init(FilePathList *list);

void file_path_list_free(FilePathList *list);

bool file_path_list_add(FilePathList *list, const char *path);

FilePathList file_list_recursively(const char *basePath);

unsigned char *file_read(const char *filename, size_t *out_size);

#endif
