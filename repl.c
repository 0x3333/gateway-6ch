#include <stdlib.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "debug.h"

int main(void)
{
  printf("9600   - %d\n", ((100 * 1000000) / 9600 + 999) / 1000);
  printf("57600  - %d\n", ((100 * 1000000) / 57600 + 999) / 1000);
  printf("115200 - %d\n", ((100 * 1000000) / 115200 + 999) / 1000);

  return 0;
}
