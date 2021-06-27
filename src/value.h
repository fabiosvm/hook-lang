//
// Hook Programming Language
// value.h
//

#ifndef VALUE_H
#define VALUE_H

#include <stdbool.h>

#define FLAG_NONE   0b00
#define FLAG_OBJECT 0b01

#define NULL_VALUE       ((value_t) {.type = TYPE_NULL, .flags = FLAG_NONE})
#define BOOLEAN_VALUE(b) ((value_t) {.type = TYPE_BOOLEAN, .flags = FLAG_NONE, .as_boolean = (b)})
#define NUMBER_VALUE(n)  ((value_t) {.type = TYPE_NUMBER, .flags = FLAG_NONE, .as_number = (n)})
#define STRING_VALUE(s)  ((value_t) {.type = TYPE_STRING, .flags = FLAG_OBJECT, .as_pointer = (s)})

#define IS_NULL(v)    ((v).type == TYPE_NULL)
#define IS_BOOLEAN(v) ((v).type == TYPE_BOOLEAN)
#define IS_NUMBER(v)  ((v).type == TYPE_NUMBER)
#define IS_OBJECT(v)  ((v).flags & FLAG_OBJECT)
#define IS_STRING(v)  ((v).type == TYPE_STRING)

#define AS_OBJECT(v) ((object_t *) (v).as_pointer)
#define AS_STRING(v) ((string_t *) (v).as_pointer)

#define OBJECT_HEADER int ref_count;

#define INCR_REF(o)       ++(o)->ref_count
#define DECR_REF(o)       --(o)->ref_count
#define IS_UNREACHABLE(o) (!(o)->ref_count)

#define VALUE_INCR_REF(v) if (IS_OBJECT(v)) INCR_REF(AS_OBJECT(v))
#define VALUE_DECR_REF(v) if (IS_OBJECT(v)) DECR_REF(AS_OBJECT(v))

typedef enum
{
  TYPE_NULL,
  TYPE_BOOLEAN,
  TYPE_NUMBER,
  TYPE_STRING
} type_t;

typedef struct
{
  type_t type;
  int flags;
  union
  {
    bool as_boolean;
    double as_number;
    void *as_pointer;
  };
} value_t;

typedef struct
{
  OBJECT_HEADER
} object_t;

const char *type_name(type_t type);
void value_free(value_t val);
void value_release(value_t val);

#endif
