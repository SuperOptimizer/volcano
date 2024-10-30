#pragma once

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