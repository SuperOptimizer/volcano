#pragma once


#include <sys/stat.h>
#include <string.h>
#include <errno.h>

#include "minilibs.h"

PRIVATE void trim(char* str) {
  char* end;
  while(isspace(*str)) str++;
  if(*str == 0) return;
  end = str + strlen(str) - 1;
  while(end > str && isspace(*end)) end--;
  end[1] = '\0';
}

PRIVATE void skip_line(FILE *fp) {
  char buffer[1024];
  fgets(buffer, sizeof(buffer), fp);
}

PRIVATE bool str_starts_with(const char* str, const char* prefix) {
  return strncmp(str, prefix, strlen(prefix)) == 0;
}


PRIVATE int mkdir_p(const char* path) {
  char tmp[1024];
  char* p = NULL;
  size_t len;

  snprintf(tmp, sizeof(tmp), "%s", path);
  len = strlen(tmp);
  if (tmp[len - 1] == '/') {
    tmp[len - 1] = 0;
  }

  for (p = tmp + 1; *p; p++) {
    if (*p == '/') {
      *p = 0;
#ifdef _WIN32
      mkdir(tmp);
#else
      mkdir(tmp, 0755);
#endif
      *p = '/';
    }
  }

#ifdef _WIN32
  return (mkdir(tmp) == 0 || errno == EEXIST) ? 0 : 1;
#else
  return (mkdir(tmp, 0755) == 0 || errno == EEXIST) ? 0 : 1;
#endif
}