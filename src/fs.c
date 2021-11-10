//
// Hook Programming Language
// fs.c
//

#include "fs.h"
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#ifdef _WIN32
#include <windows.h>
#else
#if defined(__APPLE__)
#include <sys/syslimits.h>
#else
#include <linux/limits.h>
#endif
#endif

#ifdef _WIN32
#define PATH_MAX MAX_PATH
#define mkdir _mkdir
#endif

static void make_directory(char *path);

static void make_directory(char *path)
{
  char *sep = strrchr(path, '/');
  if (sep)
  {
    *sep = '\0';
    make_directory(path);
    *sep = '/';
  }
  mkdir(path, 0777); 
}

void ensure_path(const char *filename)
{
  char *sep = strrchr(filename, '/');
  if (sep)
  {
    char path[PATH_MAX + 1];
    strncpy(path, filename, PATH_MAX);
    path[sep - filename] = '\0';
    make_directory(path);
  }
}
