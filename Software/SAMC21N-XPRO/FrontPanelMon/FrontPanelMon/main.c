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

/*
  This program is intended to reverse-engineer the data that goes over the
  bus between the front panel and the digital board microcontroller of the
  DCC-730. It probably also works on the DCC-951, FW-68 and DCC-771 since
  they all use the same hardware (there's a small chance that there are
  differences in the firmware but this is unlikely).
  
  I used a JST PH 7-pin connector and header, some ribbon cable and a
  dual row .1" header to make it easy to tap into the signals. Later on,
  I expect that I'll make a circuit that disconnects the bus if/when
  necessary and allows my microcontroller to take over some of the
  functionality of the dig-mcu.
  
  For reference, here are the signals on header 1605:
  1: SLAVE_OUT  (serial traffic from dig-mcu to front panel)
  2: GNDD       (digital ground)
  3: MESSYNC    (message sync, probably used to indicate first byte of cmd?)
  4: SLAVE_IN   (serial traffic from front panel to dig-mcu)
  5: NRESET     (reset)
  6: CLOCK      (serial clock)
  7: HOLD       (used to let the front panel wait for an operation?)
  
  With a logic analyzer, it was pretty easy to deduct that the SLAVE_OUT,
  SLAVE_IN and CLOCK lines are compatible with SPI mode 3, LSB first. My
  ancient logic analyzer can't do protocol analysis, so that's why I wrote
  this program: to figure out what the messages mean.
*/

int main(void)
{
  atmel_start_init();

  struct io_descriptor *spi_fp; // From front panel
  struct io_descriptor *spi_di; // From dig-mcu
  spi_s_async_get_io_descriptor(&SPI_EXT1, &spi_fp);
  spi_s_async_get_io_descriptor(&SPI_EXT2, &spi_di);

  spi_s_async_enable(&SPI_EXT1);
  spi_s_async_enable(&SPI_EXT2);

  printf("Front panel monitor\n");
  fflush(stdout);

  bool fromfp = false;
  
  for(;;)
  {
    uint8_t fpdata;
    uint8_t didata;

    // Data should come in on both connections at the same time.
    // So wait until there's data on both ports.
    while (!io_read(spi_fp, &fpdata, 1))
    {
      // Nothing
    }
    while (!io_read(spi_di, &didata, 1))
    {
      // Nothing
    }
    
    // If one of the bytes is 0xFF but the other one isn't, we know for sure
    // who is sending the data. Otherwise (i.e. when both incoming bytes
    // are 0xFF) we assume that the data came from the same source as the
    // previous byte.
    // That means we get out of sync if the first byte from either side is
    // 0xFF but this is very unlikely and probably easy to recognize. If it
    // happens, we're going to have to pay attention to the MESSYNC pin on
    // the deck-bridge connector.
    if (fpdata != 0xFF)
    {
      // Data came from front panel
      if (!fromfp)
      {
        printf("\n");
        fromfp = true;
      }
    }
    else if (didata != 0xFF)
    {
      // Data came from dig-mcu
      if (fromfp)
      {
        printf("-- ");
        fromfp = false;
      }
    }
    else
    {
      // Both bytes were 0xFF, don't change direction
    }
    
    printf("%02X ", (fromfp ? fpdata : didata) & 0xFF);
  }  
}
