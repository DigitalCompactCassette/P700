/**
 * \file
 *
 * \brief Application implement
 *
 * Copyright (c) 2019 Jac Goudsmit
 *
 * MIT License
 *
 */

#include "atmel_start.h"
#include "atmel_start_pins.h"
#include <string.h>
#include <stdio.h>

int main(void)
{
  atmel_start_init();

  for(;;)
  {
    printf("Hello Printf\n");
    fflush(stdout);

    delay_ms(1000);
  }  
}
