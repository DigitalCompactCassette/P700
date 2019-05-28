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

#if 0
volatile bool gotdata;

static void complete_cb_SPI_EXT1(const struct spi_s_async_descriptor *const desc)
{
  /* Transfer completed */
  gotdata = true;
}
#endif

int main(void)
{
  atmel_start_init();

  struct io_descriptor *io;
  spi_s_async_get_io_descriptor(&SPI_EXT1, &io);

//  spi_s_async_register_callback(&SPI_EXT1, SPI_S_CB_RX, (FUNC_PTR)complete_cb_SPI_EXT1);
  spi_s_async_enable(&SPI_EXT1);

  printf("Hello Printf\n");
  fflush(stdout);

  uint count = 0;
  
  for(;;)
  {
    uint8_t buf;
    
    while (io_read(io, &buf, 1))
    {
      printf("%02X ", buf & 0xFF);
      
      if (++count == 16)
      {
        printf("\n");
        count = 0;
      }
    }
    
//    delay_ms(1);
  }  
}
