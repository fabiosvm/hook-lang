//
// Hook Programming Language
// common.c
//

#include "common.h"

int nearest_power_of_two(int m, int n)
{
  int result = m;
  for (; result < n; result <<= 1);
  return result;
}
