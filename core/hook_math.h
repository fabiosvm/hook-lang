//
// Hook Programming Language
// hook_math.h
//

#ifndef HOOK_MATH_H
#define HOOK_MATH_H

#include "hook.h"

#ifdef _WIN32
int32_t __declspec(dllexport) __stdcall load_math(hk_vm_t *vm);
#else
int32_t load_math(hk_vm_t *vm);
#endif

#endif // HOOK_MATH_H
