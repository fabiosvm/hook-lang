//
// Hook Programming Language
// io.c
//

#include "io.h"
#include <stdio.h>
#include <assert.h>
#ifdef WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif
#include "common.h"
#include "error.h"

static int open_call(vm_t *vm, value_t *frame);
static int close_call(vm_t *vm, value_t *frame);
static int popen_call(vm_t *vm, value_t *frame);
static int pclose_call(vm_t *vm, value_t *frame);
static int eof_call(vm_t *vm, value_t *frame);
static int flush_call(vm_t *vm, value_t *frame);
static int sync_call(vm_t *vm, value_t *frame);
static int seek_call(vm_t *vm, value_t *frame);
static int read_call(vm_t *vm, value_t *frame);
static int write_call(vm_t *vm, value_t *frame);

static int open_call(vm_t *vm, value_t *frame)
{
  value_t val1 = frame[1];
  value_t val2 = frame[2];
  if (!IS_STRING(val1))
  {
    runtime_error("invalid type: expected string but got '%s'", type_name(val1.type));
    return STATUS_ERROR;
  }
  if (!IS_STRING(val2))
  {
    runtime_error("invalid type: expected string but got '%s'", type_name(val2.type));
    return STATUS_ERROR;
  }
  string_t *filename = AS_STRING(val1);
  string_t *mode = AS_STRING(val2);
  FILE *stream = fopen(filename->chars, mode->chars);
  if (!stream)
    vm_push_null(vm);
  return vm_push_userdata(vm, (uint64_t) stream);
}

static int close_call(vm_t *vm, value_t *frame)
{
  value_t val = frame[1];
  if (!IS_USERDATA(val))
  {
    runtime_error("invalid type: expected userdata but got '%s'", type_name(val.type));
    return STATUS_ERROR;
  }
  FILE *stream = (FILE *) val.as.userdata;
  return vm_push_number(vm, fclose(stream));
}

static int popen_call(vm_t *vm, value_t *frame)
{
  value_t val1 = frame[1];
  value_t val2 = frame[2];
  if (!IS_STRING(val1))
  {
    runtime_error("invalid type: expected string but got '%s'", type_name(val1.type));
    return STATUS_ERROR;
  }
  if (!IS_STRING(val2))
  {
    runtime_error("invalid type: expected string but got '%s'", type_name(val2.type));
    return STATUS_ERROR;
  }
  string_t *command = AS_STRING(val1);
  string_t *mode = AS_STRING(val2);
  FILE *stream;
#ifdef WIN32
  stream = _popen(command->chars, mode->chars);
#else
  stream = popen(command->chars, mode->chars);
#endif
  if (!stream)
    vm_push_null(vm);
  return vm_push_userdata(vm, (uint64_t) stream);
}

static int pclose_call(vm_t *vm, value_t *frame)
{
  value_t val = frame[1];
  if (!IS_USERDATA(val))
  {
    runtime_error("invalid type: expected userdata but got '%s'", type_name(val.type));
    return STATUS_ERROR;
  }
  FILE *stream = (FILE *) val.as.userdata;
  int status;
#ifdef WIN32
  status = _pclose(stream);
#else
  status = pclose(stream);
#endif
  return vm_push_number(vm, status);
}

static int eof_call(vm_t *vm, value_t *frame)
{
  value_t val = frame[1];
  if (!IS_USERDATA(val))
  {
    runtime_error("invalid type: expected userdata but got '%s'", type_name(val.type));
    return STATUS_ERROR;
  }
  FILE *stream = (FILE *) val.as.userdata;
  return vm_push_boolean(vm, (bool) feof(stream));
}

static int flush_call(vm_t *vm, value_t *frame)
{
  value_t val = frame[1];
  if (!IS_USERDATA(val))
  {
    runtime_error("invalid type: expected userdata but got '%s'", type_name(val.type));
    return STATUS_ERROR;
  }
  FILE *stream = (FILE *) val.as.userdata;
  return vm_push_number(vm, fflush(stream));
}

static int sync_call(vm_t *vm, value_t *frame)
{
  value_t val = frame[1];
  if (!IS_USERDATA(val))
  {
    runtime_error("invalid type: expected userdata but got '%s'", type_name(val.type));
    return STATUS_ERROR;
  }
  FILE *stream = (FILE *) val.as.userdata;
  int fd = fileno(stream);
  bool result;
#ifdef WIN32
  result = FlushFileBuffers(fd);
#else
  result = !fsync(fd);
#endif
  return vm_push_boolean(vm, result);
}

static int seek_call(vm_t *vm, value_t *frame)
{
  value_t val1 = frame[1];
  value_t val2 = frame[2];
  value_t val3 = frame[3];
  if (!IS_USERDATA(val1))
  {
    runtime_error("invalid type: expected userdata but got '%s'", type_name(val1.type));
    return STATUS_ERROR;
  }
  if (!IS_INTEGER(val2))
  {
    runtime_error("invalid type: expected integer but got '%s'", type_name(val2.type));
    return STATUS_ERROR;
  }
  if (!IS_INTEGER(val3))
  {
    runtime_error("invalid type: expected integer but got '%s'", type_name(val3.type));
    return STATUS_ERROR;
  }
  FILE *stream = (FILE *) val1.as.userdata;
  long offset = (long) val2.as.number;
  int whence = (int) val3.as.number;
  return vm_push_number(vm, fseek(stream, offset, whence));
}

static int read_call(vm_t *vm, value_t *frame)
{
  value_t val1 = frame[1];
  value_t val2 = frame[2];
  if (!IS_USERDATA(val1))
  {
    runtime_error("invalid type: expected userdata but got '%s'", type_name(val1.type));
    return STATUS_ERROR;
  }
  if (!IS_INTEGER(val2))
  {
    runtime_error("invalid type: expected integer but got '%s'", type_name(val2.type));
    return STATUS_ERROR;
  }
  FILE *stream = (FILE *) val1.as.userdata;
  long size = (long) val2.as.number;
  string_t *str = string_allocate(size);
  int length = (int) fread(str->chars, 1, size, stream);
  if (length < size && !feof(stream))
  {
    string_free(str);
    return vm_push_null(vm);
  }
  str->length = length;
  return vm_push_string(vm, str);
}

static int write_call(vm_t *vm, value_t *frame)
{
  value_t val1 = frame[1];
  value_t val2 = frame[2];
  if (!IS_USERDATA(val1))
  {
    runtime_error("invalid type: expected userdata but got '%s'", type_name(val1.type));
    return STATUS_ERROR;
  }
  if (!IS_STRING(val2))
  {
    runtime_error("invalid type: expected string but got '%s'", type_name(val2.type));
    return STATUS_ERROR;
  }
  FILE *stream = (FILE *) val1.as.userdata;
  string_t *str = AS_STRING(val2);
  int size = str->length;
  int length = (int) fwrite(str->chars, 1, size, stream);
  if (length < size)
    return vm_push_null(vm);
  return vm_push_number(vm, length);
}

#ifdef WIN32
void __declspec(dllexport) __stdcall load_io(vm_t *vm)
#else
void load_io(vm_t *vm)
#endif
{
  char std_in[] = "STD_IN";
  char std_out[] = "STD_OUT";
  char std_err[] = "STD_ERR";
  char seek_set[] = "SEEK_SET";
  char seek_current[] = "SEEK_CURRENT";
  char seek_end[] = "SEEK_END";
  char open[] = "open";
  char close[] = "close";
  char popen[] = "popen";
  char pclose[] = "pclose";
  char eof[] = "eof";
  char flush[] = "flush";
  char sync[] = "sync";
  char seek[] = "seek";
  char read[] = "read";
  char write[] = "write";
  struct_t *ztruct = struct_new(string_from_chars(-1, "io"));
  assert(struct_put_if_absent(ztruct, sizeof(std_in) - 1, std_in));
  assert(struct_put_if_absent(ztruct, sizeof(std_out) - 1, std_out));
  assert(struct_put_if_absent(ztruct, sizeof(std_err) - 1, std_err));
  assert(struct_put_if_absent(ztruct, sizeof(seek_set) - 1, seek_set));
  assert(struct_put_if_absent(ztruct, sizeof(seek_current) - 1, seek_current));
  assert(struct_put_if_absent(ztruct, sizeof(seek_end) - 1, seek_end));
  assert(struct_put_if_absent(ztruct, sizeof(open) - 1, open));
  assert(struct_put_if_absent(ztruct, sizeof(close) - 1, close));
  assert(struct_put_if_absent(ztruct, sizeof(popen) - 1, popen));
  assert(struct_put_if_absent(ztruct, sizeof(pclose) - 1, pclose));
  assert(struct_put_if_absent(ztruct, sizeof(eof) - 1, eof));
  assert(struct_put_if_absent(ztruct, sizeof(flush) - 1, flush));
  assert(struct_put_if_absent(ztruct, sizeof(sync) - 1, sync));
  assert(struct_put_if_absent(ztruct, sizeof(seek) - 1, seek));
  assert(struct_put_if_absent(ztruct, sizeof(read) - 1, read));
  assert(struct_put_if_absent(ztruct, sizeof(write) - 1, write));
  assert(vm_push_userdata(vm, (uint64_t) stdin) == STATUS_OK);
  assert(vm_push_userdata(vm, (uint64_t) stdout) == STATUS_OK);
  assert(vm_push_userdata(vm, (uint64_t) stderr) == STATUS_OK);
  assert(vm_push_number(vm, SEEK_SET) == STATUS_OK);
  assert(vm_push_number(vm, SEEK_CUR) == STATUS_OK);
  assert(vm_push_number(vm, SEEK_END) == STATUS_OK);
  assert(vm_push_native(vm, native_new(string_from_chars(-1, open), 2, &open_call)) == STATUS_OK);
  assert(vm_push_native(vm, native_new(string_from_chars(-1, close), 1, &close_call)) == STATUS_OK);
  assert(vm_push_native(vm, native_new(string_from_chars(-1, popen), 2, &popen_call)) == STATUS_OK);
  assert(vm_push_native(vm, native_new(string_from_chars(-1, pclose), 1, &pclose_call)) == STATUS_OK);
  assert(vm_push_native(vm, native_new(string_from_chars(-1, eof), 1, &eof_call)) == STATUS_OK);
  assert(vm_push_native(vm, native_new(string_from_chars(-1, flush), 1, &flush_call)) == STATUS_OK);
  assert(vm_push_native(vm, native_new(string_from_chars(-1, sync), 1, &sync_call)) == STATUS_OK);
  assert(vm_push_native(vm, native_new(string_from_chars(-1, seek), 3, &seek_call)) == STATUS_OK);
  assert(vm_push_native(vm, native_new(string_from_chars(-1, read), 2, &read_call)) == STATUS_OK);
  assert(vm_push_native(vm, native_new(string_from_chars(-1, write), 2, &write_call)) == STATUS_OK);
  assert(vm_push_struct(vm, ztruct) == STATUS_OK);
  vm_instance(vm);
}
