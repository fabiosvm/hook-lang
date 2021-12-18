//
// Hook Programming Language
// callable.h
//

#ifndef CALLABLE_H
#define CALLABLE_H

#include "string.h"
#include "chunk.h"
#include "array.h"

typedef struct
{
  int no;
  int offset;
} line_t;

typedef struct prototype
{
  OBJECT_HEADER
  int arity;
  string_t *name;
  string_t *file;
  int lines_capacity;
  int num_lines;
  line_t *lines;
  chunk_t chunk;
  array_t *consts;
  uint8_t protos_capacity;
  uint8_t num_protos;
  struct prototype **protos;
  uint8_t num_nonlocals;
} prototype_t;

typedef struct
{
  OBJECT_HEADER
  prototype_t *proto;
  value_t nonlocals[0];
} function_t;

struct vm;

typedef struct
{
  OBJECT_HEADER
  int arity;
  string_t *name;
  int (*call)(struct vm *, value_t *);
} native_t;

prototype_t *prototype_new(int arity, string_t *name, string_t *file);
void prototype_free(prototype_t *proto);
void prototype_add_line(prototype_t *proto, int line_no);
int prototype_get_line(prototype_t *proto, int offset);
void prototype_add_child(prototype_t *proto, prototype_t *child);
void prototype_serialize(prototype_t *proto, FILE *stream);
prototype_t *prototype_deserialize(FILE *stream);
function_t *function_new(prototype_t *proto);
void function_free(function_t *fn);
native_t *native_new(string_t *name, int arity, int (*call)(struct vm *, value_t *));
void native_free(native_t *native);

#endif // CALLABLE_H
