#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

static void trim(char* str) {
  char* end;
  while(isspace(*str)) str++;
  if(*str == 0) return;
  end = str + strlen(str) - 1;
  while(end > str && isspace(*end)) end--;
  end[1] = '\0';
}
