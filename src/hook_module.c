//
// Hook Programming Language
// hook_module.c
//

#include "hook_module.h"
#include <stdlib.h>
#ifdef _WIN32
#include <Windows.h>
#else
#include <dlfcn.h>
#endif
#include "hook_string_map.h"
#include "hook_utils.h"
#include "hook_status.h"
#include "hook_error.h"

#define HOME_VAR "HOOK_HOME"

#ifdef _WIN32
#define FILE_INFIX "\\lib\\"
#define FILE_EXT   ".dll"
#else
#define FILE_INFIX "/lib/lib"
#ifdef __APPLE__
#define FILE_EXT   ".dylib"
#else
#define FILE_EXT   ".so"
#endif
#endif

#define FUNC_PREFIX "load_"

#ifdef _WIN32
typedef int32_t (__stdcall *load_module_t)(hk_vm_t *);
#else
typedef int32_t (*load_module_t)(hk_vm_t *);
#endif

static string_map_t module_cache;

static inline bool get_module_result(hk_string_t *name, hk_value_t *result);
static inline void put_module_result(hk_string_t *name, hk_value_t result);
static inline const char *get_home_dir(void);
static inline int32_t load_native_module(hk_vm_t *vm, hk_string_t *name);

static inline bool get_module_result(hk_string_t *name, hk_value_t *result)
{
  string_map_entry_t *entry = string_map_get_entry(&module_cache, name);
  if (!entry)
    return false;
  *result = entry->value;
  return true;
}

static inline void put_module_result(hk_string_t *name, hk_value_t result)
{
  string_map_inplace_put(&module_cache, name, result);
}

static inline const char *get_home_dir(void)
{
  const char *home_dir = getenv(HOME_VAR);
  if (!home_dir)
  {
#ifdef _WIN32
    const char *drive = getenv("SystemDrive");
    hk_assert(drive, "environment variable 'SystemDrive' not set");
    char *path[MAX_PATH + 1];
    snprintf(path, MAX_PATH, "%s\\hook", drive);
    strncpy(path, drive, MAX_PATH);
    home_dir = (const char *) path;
#else
    home_dir = "/usr/local/hook";
#endif
  }
  return home_dir;
}

static inline int32_t load_native_module(hk_vm_t *vm, hk_string_t *name)
{
  hk_string_t *file = hk_string_from_chars(-1, get_home_dir());
  hk_string_inplace_concat_chars(file, -1, FILE_INFIX);
  hk_string_inplace_concat(file, name);
  hk_string_inplace_concat_chars(file, -1, FILE_EXT);
#ifdef _WIN32
  HINSTANCE handle = LoadLibrary(file->chars);
#else
  void *handle = dlopen(file->chars, RTLD_NOW | RTLD_GLOBAL);
#endif
  if (!handle)
  {
    hk_runtime_error("cannot open module `%.*s`", name->length, name->chars);
    hk_string_free(file);
    return HK_STATUS_ERROR;
  }
  hk_string_free(file);
  hk_string_t *func = hk_string_from_chars(-1, FUNC_PREFIX);
  hk_string_inplace_concat(func, name);
#ifdef _WIN32
  load_module_t load = (load_module_t) GetProcAddress(handle, func->chars);
#else
  load_module_t load = dlsym(handle, func->chars);
#endif
  if (!load)
  {
    hk_runtime_error("no such function %.*s()", func->length, func->chars);
    hk_string_free(func);
    return HK_STATUS_ERROR;
  }
  hk_string_free(func);
  if (load(vm) == HK_STATUS_ERROR)
  {
    hk_runtime_error("cannot load module `%.*s`", name->length, name->chars);
    return HK_STATUS_ERROR;
  }
  return HK_STATUS_OK;
}

void init_module_cache(void)
{
  string_map_init(&module_cache, 0);
}

void free_module_cache(void)
{
  string_map_free(&module_cache);
}

int32_t load_module(hk_vm_t *vm)
{
  hk_value_t *slots = &vm->stack[vm->stack_top];
  hk_value_t val = slots[0];
  hk_assert(hk_is_string(val), "module name must be a string");
  hk_string_t *name = hk_as_string(val);
  hk_value_t result;
  if (get_module_result(name, &result))
  {
    hk_value_incr_ref(result);
    slots[0] = result;
    --vm->stack_top;
    hk_string_release(name);
    return HK_STATUS_OK;
  }
  if (load_native_module(vm, name) == HK_STATUS_ERROR)
    return HK_STATUS_ERROR;
  put_module_result(name, vm->stack[vm->stack_top]);
  slots[0] = vm->stack[vm->stack_top];
  --vm->stack_top;
  hk_string_release(name);
  return HK_STATUS_OK;
}
