//
// Hook Programming Language
// string.h
//

#ifndef STRING_H
#define STRING_H

#include <stdio.h>
#include <stdint.h>
#include "value.h"

#define STRING_MIN_CAPACITY 8

typedef struct
{
  OBJECT_HEADER
  int capacity;
  char *chars;
  int length;
} string_t;

string_t *string_from_chars(int length, const char *chars);
string_t *string_from_stream(FILE *fp);
void string_free(string_t *str);

#endif
