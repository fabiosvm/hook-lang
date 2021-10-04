//
// Hook Programming Language
// disasm.c
//

#include "disasm.h"
#include <stdio.h>

void dump(function_t *fn)
{
  printf("<function %s at %p>\n", fn->name->chars, fn);
  uint8_t *bytes = fn->chunk.bytes;
  int i = 0;
  int n = 0;
  while (i < fn->chunk.length)
  {
    opcode_t op = (opcode_t) bytes[i];
    int j = i++;
    ++n;
    switch (op)
    {
    case OP_NULL:
      printf("[%05d] Null\n", j);
      break;
    case OP_FALSE:
      printf("[%05d] False\n", j);
      break;
    case OP_TRUE:
      printf("[%05d] True\n", j);
      break;
    case OP_INT:
      {
        int data = *((uint16_t*) &bytes[i]);
        i += 2;
        printf("[%05d] Int               %d\n", j, data);
      }
      break;
    case OP_CONSTANT:
      printf("[%05d] Constant          %d\n", j, bytes[i++]);
      break;
    case OP_ARRAY:
      printf("[%05d] Array             %d\n", j, bytes[i++]);
      break;
    case OP_UNPACK:
      printf("[%05d] Unpack            %d\n", j, bytes[i++]);
      break;
    case OP_POP:
      printf("[%05d] Pop\n", j);
      break;
    case OP_GET_LOCAL:
      printf("[%05d] GetLocal          %d\n", j, bytes[i++]);
      break;
    case OP_SET_LOCAL:
      printf("[%05d] SetLocal          %d\n", j, bytes[i++]);
      break;
    case OP_GET_ELEMENT:
      printf("[%05d] GetElement\n", j);
      break;
    case OP_INPLACE_APPEND:
      printf("[%05d] InplaceAppend\n", j);
      break;
    case OP_INPLACE_PUT_ELEMENT:
      printf("[%05d] InplacePutElement\n", j);
      break;
    case OP_INPLACE_DELETE:
      printf("[%05d] InplaceDelete\n", j);
      break;
    case OP_JUMP:
      {
        int offset = *((uint16_t*) &bytes[i]);
        i += 2;
        printf("[%05d] Jump              %d\n", j, offset);
      }
      break;
    case OP_JUMP_IF_FALSE:
      {
        int offset = *((uint16_t*) &bytes[i]);
        i += 2;
        printf("[%05d] JumpIfFalse       %d\n", j, offset);
      }
      break;
    case OP_JUMP_IF_TRUE:
      {
        int offset = *((uint16_t*) &bytes[i]);
        i += 2;
        printf("[%05d] JumpIfTrue        %d\n", j, offset);
      }
      break;
    case OP_EQUAL:
      printf("[%05d] Equal\n", j);
      break;
    case OP_GREATER:
      printf("[%05d] Greater\n", j);
      break;
    case OP_LESS:
      printf("[%05d] Less\n", j);
      break;
    case OP_ADD:
      printf("[%05d] Add\n", j);
      break;
    case OP_SUBTRACT:
      printf("[%05d] Subtract\n", j);
      break;
    case OP_MULTIPLY:
      printf("[%05d] Multiply\n", j);
      break;
    case OP_DIVIDE:
      printf("[%05d] Divide\n", j);
      break;
    case OP_MODULO:
      printf("[%05d] Modulo\n", j);
      break;
    case OP_NEGATE:
      printf("[%05d] Negate\n", j);
      break;
    case OP_NOT:
      printf("[%05d] Not\n", j);
      break;
    case OP_PRINT:
      printf("[%05d] Print\n", j);
      break;
    case OP_RETURN:
      printf("[%05d] Return\n", j);
      break;
    }
  }
  printf("%d instruction(s)\n\n", n);
}
