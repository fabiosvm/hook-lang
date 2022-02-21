//
// Hook Programming Language
// builtin.c
//

#include "builtin.h"
#include <stdlib.h>
#include <string.h>
#include <float.h>
#include <ctype.h>
#include <limits.h>
#ifdef _WIN32
#include <Windows.h>
#else
#include <unistd.h>
#endif
#include <errno.h>
#include "struct.h"
#include "common.h"
#include "error.h"

#ifdef _WIN32
#define strtok_r strtok_s
#endif

static const char *globals[] = {
  "print",
  "println",
  "type",
  "bool",
  "integer",
  "int",
  "num",
  "str",
  "ord",
  "chr",
  "hex",
  "bin",
  "cap",
  "len",
  "is_empty",
  "compare",
  "slice",
  "split",
  "join",
  "iter",
  "valid",
  "current",
  "next",
  "sleep",
  "assert",
  "panic"
};

static inline int string_to_double(string_t *str, double *result);
static inline array_t *split(string_t *str, string_t *separator);
static inline int join(array_t *arr, string_t *separator, string_t **result);
static int print_call(vm_t *vm, value_t *args);
static int println_call(vm_t *vm, value_t *args);
static int type_call(vm_t *vm, value_t *args);
static int bool_call(vm_t *vm, value_t *args);
static int integer_call(vm_t *vm, value_t *args);
static int int_call(vm_t *vm, value_t *args);
static int num_call(vm_t *vm, value_t *args);
static int str_call(vm_t *vm, value_t *args);
static int ord_call(vm_t *vm, value_t *args);
static int chr_call(vm_t *vm, value_t *args);
static int hex_call(vm_t *vm, value_t *args);
static int bin_call(vm_t *vm, value_t *args);
static int cap_call(vm_t *vm, value_t *args);
static int len_call(vm_t *vm, value_t *args);
static int is_empty_call(vm_t *vm, value_t *args);
static int compare_call(vm_t *vm, value_t *args);
static int slice_call(vm_t *vm, value_t *args);
static int split_call(vm_t *vm, value_t *args);
static int join_call(vm_t *vm, value_t *args);
static int iter_call(vm_t *vm, value_t *args);
static int valid_call(vm_t *vm, value_t *args);
static int current_call(vm_t *vm, value_t *args);
static int next_call(vm_t *vm, value_t *args);
static int sleep_call(vm_t *vm, value_t *args);
static int assert_call(vm_t *vm, value_t *args);
static int panic_call(vm_t *vm, value_t *args);

static inline int string_to_double(string_t *str, double *result)
{
  if (!str->length)
  {
    runtime_error("type error: argument #1 must be a non-empty string");
    return STATUS_ERROR;
  }
  errno = 0;
  char *ptr;
  *result = strtod(str->chars, &ptr);
  if (errno == ERANGE)
  {
    runtime_error("type error: argument #1 is a too large string");
    return STATUS_ERROR;
  }
  while (*ptr != 0 && isspace(*ptr))
    ++ptr;
  if (ptr < &str->chars[str->length])
  {
    runtime_error("type error: argument #1 is not a convertible string");
    return STATUS_ERROR;
  }
  return STATUS_OK;
}

static inline array_t *split(string_t *str, string_t *separator)
{
  array_t *arr = array_new(0);
  char *cur = str->chars;
  char *tk;
  while ((tk = strtok_r(cur, separator->chars, &cur)))
  {
    value_t elem = STRING_VALUE(string_from_chars(-1, tk));
    array_inplace_add_element(arr, elem);
  }
  return arr;
}

static inline int join(array_t *arr, string_t *separator, string_t **result)
{
  string_t *str = string_new(0);
  for (int i = 0; i < arr->length; ++i)
  {
    value_t elem = arr->elements[i];
    if (!IS_STRING(elem))
      continue;
    if (i)
      string_inplace_concat(str, separator);
    string_inplace_concat(str, AS_STRING(elem));
  }
  *result = str;
  return STATUS_OK;
}

static int print_call(vm_t *vm, value_t *args)
{
  value_print(args[1], false);
  return vm_push_nil(vm);
}

static int println_call(vm_t *vm, value_t *args)
{
  value_print(args[1], false);
  printf("\n");
  return vm_push_nil(vm);
}

static int type_call(vm_t *vm, value_t *args)
{
  return vm_push_string_from_chars(vm, -1, type_name(args[1].type));
}

static int bool_call(vm_t *vm, value_t *args)
{
  return vm_push_boolean(vm, IS_TRUTHY(args[1]));
}

static int integer_call(vm_t *vm, value_t *args)
{
  type_t types[] = {TYPE_NUMBER, TYPE_STRING};
  if (vm_check_types(args, 1, 2, types) == STATUS_ERROR)
    return STATUS_ERROR;
  value_t val = args[1];
  if (IS_NUMBER(val))
    return vm_push_number(vm, (long) val.as.number);
  double result;
  if (string_to_double(AS_STRING(val), &result) == STATUS_ERROR)
    return STATUS_ERROR;
  return vm_push_number(vm, (long) result);
}

static int int_call(vm_t *vm, value_t *args)
{
  type_t types[] = {TYPE_NUMBER, TYPE_STRING};
  if (vm_check_types(args, 1, 2, types) == STATUS_ERROR)
    return STATUS_ERROR;
  value_t val = args[1];
  if (IS_NUMBER(val))
    return vm_push_number(vm, (int) val.as.number);
  double result;
  if (string_to_double(AS_STRING(val), &result) == STATUS_ERROR)
    return STATUS_ERROR;
  return vm_push_number(vm, (int) result);
}

static int num_call(vm_t *vm, value_t *args)
{
  type_t types[] = {TYPE_NUMBER, TYPE_STRING};
  if (vm_check_types(args, 1, 2, types) == STATUS_ERROR)
    return STATUS_ERROR;
  value_t val = args[1];
  if (IS_NUMBER(val))
    return STATUS_OK;
  double result;
  if (string_to_double(AS_STRING(args[1]), &result) == STATUS_ERROR)
    return STATUS_ERROR;
  return vm_push_number(vm, result);
}

static int str_call(vm_t *vm, value_t *args)
{
  type_t types[] = {TYPE_NIL, TYPE_BOOLEAN, TYPE_NUMBER, TYPE_STRING};
  if (vm_check_types(args, 1, 4, types) == STATUS_ERROR)
    return STATUS_ERROR;
  value_t val = args[1];
  string_t *str;
  if (IS_NIL(val))
  {
    str = string_from_chars(-1, "nil");
    goto end;
  }
  if (IS_BOOLEAN(val))
  {
    str = string_from_chars(-1, val.as.boolean ? "true" : "false");
    goto end;
  }
  if (IS_NUMBER(val))
  {
    char chars[32];
    sprintf(chars, "%g", val.as.number);
    str = string_from_chars(-1, chars);
    goto end;
  }
  return vm_push(vm, val);
end:
  if (vm_push_string(vm, str) == STATUS_ERROR)
  {
    string_free(str);
    return STATUS_ERROR;
  }
  return STATUS_OK;
}

static int ord_call(vm_t *vm, value_t *args)
{
  if (vm_check_string(args, 1) == STATUS_ERROR)
    return STATUS_ERROR;
  value_t val = args[1];
  string_t *str = AS_STRING(val);
  if (!str->length)
  {
    runtime_error("type error: argument #1 must be a non-empty string");
    return STATUS_ERROR;
  }
  return vm_push_number(vm, (unsigned int) str->chars[0]);
}

static int chr_call(vm_t *vm, value_t *args)
{
  value_t val = args[1];
  if (vm_check_int(args, 1) == STATUS_ERROR)
    return STATUS_ERROR;
  int data = (int) val.as.number;
  if (data < 0 || data > UCHAR_MAX)
  {
    runtime_error("range error: argument #1 must be between 0 and %d", UCHAR_MAX);
    return STATUS_ERROR;
  }
  string_t *str = string_allocate(1);
  str->length = 1;
  str->chars[0] = (char) data;
  str->chars[1] = '\0';
  if (vm_push_string(vm, str) == STATUS_ERROR)
  {
    string_free(str);
    return STATUS_ERROR;
  }
  return STATUS_OK;
}

static int hex_call(vm_t *vm, value_t *args)
{
  if (vm_check_string(args, 1) == STATUS_ERROR)
    return STATUS_ERROR;
  string_t *str = AS_STRING(args[1]);
  if (!str->length)
    return vm_push_string(vm, str);
  int length = str->length << 1;
  string_t *result = string_allocate(length);
  result->length = length;
  result->chars[length] = '\0';
  char *chars = result->chars;
  for (int i = 0; i < str->length; ++i)
  {
    sprintf(chars, "%.2x", (unsigned char) str->chars[i]);
    chars += 2;
  }
  if (vm_push_string(vm, result) == STATUS_ERROR)
  {
    string_free(result);
    return STATUS_ERROR;
  }
  return STATUS_OK;
}

static int bin_call(vm_t *vm, value_t *args)
{
  if (vm_check_string(args, 1) == STATUS_ERROR)
    return STATUS_ERROR;
  string_t *str = AS_STRING(args[1]);
  if (!str->length)
    return vm_push_string(vm, str);
  if (str->length % 2)
  {
    vm_push_nil(vm);
    return STATUS_OK;
  }
  int length = str->length >> 1;
  string_t *result = string_allocate(length);
  result->length = length;
  result->chars[length] = '\0';
  char *chars = str->chars;
  for (int i = 0; i < length; ++i)
  {
    sscanf(chars, "%2hhx", &result->chars[i]);
    chars += 2;
  }
  if (vm_push_string(vm, result) == STATUS_ERROR)
  {
    string_free(result);
    return STATUS_ERROR;
  }
  return STATUS_OK;
}

static int cap_call(vm_t *vm, value_t *args)
{
  type_t types[] = {TYPE_STRING, TYPE_ARRAY};
  if (vm_check_types(args, 1, 2, types) == STATUS_ERROR)
    return STATUS_ERROR;
  value_t val = args[1];
  int capacity = IS_STRING(val) ? AS_STRING(val)->capacity
    : AS_ARRAY(val)->capacity;
  return vm_push_number(vm, capacity);
}

static int len_call(vm_t *vm, value_t *args)
{
  type_t types[] = {TYPE_STRING, TYPE_RANGE, TYPE_ARRAY,
    TYPE_STRUCT, TYPE_INSTANCE};
  if (vm_check_types(args, 1, 5, types) == STATUS_ERROR)
    return STATUS_ERROR;
  value_t val = args[1];
  if (IS_STRING(val))
    return vm_push_number(vm, AS_STRING(val)->length);
  if (IS_RANGE(val))
  {
    range_t *range = AS_RANGE(val);
    if (range->start < range->end)
    {
      int result = (int) range->end - range->start + 1;
      return vm_push_number(vm, result);
    }
    if (range->start > range->end)
    {
      int result = (int) range->start - range->end + 1;
      return vm_push_number(vm, result);
    }
    return vm_push_number(vm, 1);
  }
  if (IS_ARRAY(val))
    return vm_push_number(vm, AS_ARRAY(val)->length);
  if (IS_STRUCT(val))
    return vm_push_number(vm, AS_STRUCT(val)->length);
  return vm_push_number(vm, AS_INSTANCE(val)->ztruct->length);
}

static int is_empty_call(vm_t *vm, value_t *args)
{
  type_t types[] = {TYPE_STRING, TYPE_RANGE, TYPE_ARRAY,
    TYPE_STRUCT, TYPE_INSTANCE};
  if (vm_check_types(args, 1, 5, types) == STATUS_ERROR)
    return STATUS_ERROR;
  value_t val = args[1];
  if (IS_STRING(val))
    return vm_push_boolean(vm, !AS_STRING(val)->length);
  if (IS_RANGE(val))
    return vm_push_boolean(vm, false);
  if (IS_ARRAY(val))
    return vm_push_boolean(vm, !AS_ARRAY(val)->length);
  if (IS_STRUCT(val))
    return vm_push_boolean(vm, !AS_STRUCT(val)->length);
  return vm_push_boolean(vm, !AS_INSTANCE(val)->ztruct->length);
}

static int compare_call(vm_t *vm, value_t *args)
{
  value_t val1 = args[1];
  value_t val2 = args[2];
  int result;
  if (value_compare(val1, val2, &result) == STATUS_ERROR)
    return STATUS_ERROR;
  return vm_push_number(vm, result);
}

static int slice_call(vm_t *vm, value_t *args)
{
  type_t types[] = {TYPE_STRING, TYPE_ARRAY};
  if (vm_check_types(args, 1, 2, types) == STATUS_ERROR)
    return STATUS_ERROR;
  if (vm_check_int(args, 2) == STATUS_ERROR)
    return STATUS_ERROR;
  if (vm_check_int(args, 3) == STATUS_ERROR)
    return STATUS_ERROR;
  value_t val = args[1];
  int start = (int) args[2].as.number;
  int stop = (int) args[3].as.number;
  if (IS_STRING(val))
  {
    string_t *str = AS_STRING(val);
    string_t *result;
    if (!string_slice(str, start, stop, &result))
    {
      vm_pop(vm);
      vm_pop(vm);
      return STATUS_OK;
    }
    if (vm_push_string(vm, result) == STATUS_ERROR)
    {
      string_free(result);
      return STATUS_ERROR;
    }
    return STATUS_OK;
  }
  array_t *arr = AS_ARRAY(val);
  array_t *result;
  if (!array_slice(arr, start, stop, &result))
  {
    vm_pop(vm);
    vm_pop(vm);
    return STATUS_OK;
  }
  if (vm_push_array(vm, result) == STATUS_ERROR)
  {
    array_free(result);
    return STATUS_ERROR;
  }
  return STATUS_OK;
}

static int split_call(vm_t *vm, value_t *args)
{
  if (vm_check_type(args, 1, TYPE_STRING) == STATUS_ERROR)
    return STATUS_ERROR;
  if (vm_check_type(args, 2, TYPE_STRING) == STATUS_ERROR)
    return STATUS_ERROR;
  array_t *arr = split(AS_STRING(args[1]), AS_STRING(args[2]));
  if (vm_push_array(vm, arr) == STATUS_ERROR)
  {
    array_free(arr);
    return STATUS_ERROR;
  }
  return STATUS_OK;
}

static int join_call(vm_t *vm, value_t *args)
{
  if (vm_check_type(args, 1, TYPE_ARRAY) == STATUS_ERROR)
    return STATUS_ERROR;
  if (vm_check_type(args, 2, TYPE_STRING) == STATUS_ERROR)
    return STATUS_ERROR;
  string_t *str;
  if (join(AS_ARRAY(args[1]), AS_STRING(args[2]), &str) == STATUS_ERROR)
    return STATUS_ERROR;
  if (vm_push_string(vm, str) == STATUS_ERROR)
  {
    string_free(str);
    return STATUS_ERROR;
  }
  return STATUS_OK;
}

static int iter_call(vm_t *vm, value_t *args)
{
  type_t types[] = {TYPE_ITERATOR, TYPE_RANGE, TYPE_ARRAY};
  if (vm_check_types(args, 1, 3, types) == STATUS_ERROR)
    return STATUS_ERROR;
  value_t val = args[1];
  if (IS_ITERATOR(val))
  {
    if (vm_push_iterator(vm, AS_ITERATOR(val)) == STATUS_ERROR)
      return STATUS_ERROR;
    return STATUS_OK;
  }
  iterator_t *it = IS_RANGE(val) ? range_new_iterator(AS_RANGE(val))
    : array_new_iterator(AS_ARRAY(val));
  if (vm_push_iterator(vm, it) == STATUS_ERROR)
  {
    iterator_free(it);
    return STATUS_ERROR;
  }
  return STATUS_OK;
}

static int valid_call(vm_t *vm, value_t *args)
{
  if (vm_check_type(args, 1, TYPE_ITERATOR) == STATUS_ERROR)
    return STATUS_ERROR;
  return vm_push_boolean(vm, iterator_is_valid(AS_ITERATOR(args[1])));
}

static int current_call(vm_t *vm, value_t *args)
{
  if (vm_check_type(args, 1, TYPE_ITERATOR) == STATUS_ERROR)
    return STATUS_ERROR;
  iterator_t *it = AS_ITERATOR(args[1]);
  if (!iterator_is_valid(it))
    return vm_push_nil(vm);
  return vm_push(vm, iterator_get_current(it));
}

static int next_call(vm_t *vm, value_t *args)
{
  if (vm_check_type(args, 1, TYPE_ITERATOR) == STATUS_ERROR)
    return STATUS_ERROR;
  iterator_t *it = AS_ITERATOR(args[1]);
  if (iterator_is_valid(it))
    iterator_next(it);
  return vm_push_nil(vm);
}

static int sleep_call(vm_t *vm, value_t *args)
{
  if (vm_check_int(args, 1) == STATUS_ERROR)
    return STATUS_ERROR;
  int ms = (int) args[1].as.number;
#ifdef _WIN32
  Sleep(ms);
#else
  ASSERT(!usleep(ms * 1000), "unexpected error on usleep()");
#endif
  return vm_push_nil(vm);
}

static int assert_call(vm_t *vm, value_t *args)
{
  if (vm_check_string(args, 2) == STATUS_ERROR)
    return STATUS_ERROR;
  if (IS_FALSEY(args[1]))
  {
    string_t *str = AS_STRING(args[2]);
    fprintf(stderr, "assertion failed: %.*s\n", str->length, str->chars);
    return STATUS_NO_TRACE;
  }
  return vm_push_nil(vm);
}

static int panic_call(vm_t *vm, value_t *args)
{
  (void) vm;
  if (vm_check_string(args, 1) == STATUS_ERROR)
    return STATUS_ERROR;
  string_t *str = AS_STRING(args[1]);
  fprintf(stderr, "panic: %.*s\n", str->length, str->chars);
  return STATUS_NO_TRACE;
}

void load_globals(vm_t *vm)
{
  vm_push_new_native(vm, globals[0], 1, &print_call);
  vm_push_new_native(vm, globals[1], 1, &println_call);
  vm_push_new_native(vm, globals[2], 1, &type_call);
  vm_push_new_native(vm, globals[3], 1, &bool_call);
  vm_push_new_native(vm, globals[4], 1, &integer_call);
  vm_push_new_native(vm, globals[5], 1, &int_call);
  vm_push_new_native(vm, globals[6], 1, &num_call);
  vm_push_new_native(vm, globals[7], 1, &str_call);
  vm_push_new_native(vm, globals[8], 1, &ord_call);
  vm_push_new_native(vm, globals[9], 1, &chr_call);
  vm_push_new_native(vm, globals[10], 1, &hex_call);
  vm_push_new_native(vm, globals[11], 1, &bin_call);
  vm_push_new_native(vm, globals[12], 1, &cap_call);
  vm_push_new_native(vm, globals[13], 1, &len_call);
  vm_push_new_native(vm, globals[14], 1, &is_empty_call);
  vm_push_new_native(vm, globals[15], 2, &compare_call);
  vm_push_new_native(vm, globals[16], 3, &slice_call);
  vm_push_new_native(vm, globals[17], 2, &split_call);
  vm_push_new_native(vm, globals[18], 2, &join_call);
  vm_push_new_native(vm, globals[19], 1, &iter_call);
  vm_push_new_native(vm, globals[20], 1, &valid_call);
  vm_push_new_native(vm, globals[21], 1, &current_call);
  vm_push_new_native(vm, globals[22], 1, &next_call);
  vm_push_new_native(vm, globals[23], 1, &sleep_call);
  vm_push_new_native(vm, globals[24], 2, &assert_call);
  vm_push_new_native(vm, globals[25], 1, &panic_call);
}

int num_globals(void)
{
  return (int) (sizeof(globals) / sizeof(*globals));
}

int lookup_global(int length, char *chars)
{
  int index = num_globals() - 1;
  for (; index > -1; --index)
  {
    const char *global = globals[index];
    if (!strncmp(global, chars, length) && !global[length])
      break;
  }
  return index;
}
