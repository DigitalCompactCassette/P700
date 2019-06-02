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


#define DEBOUNCE_COUNT 5


//---------------------------------------------------------------------------
// Check the button and disable the SPI ports if it's pressed.
//
// The SPI ports are enabled again when the button is released.
//
// Returns true if the function is debouncing the switch input.
bool check_button(void)
{
  static unsigned count;

  if (gpio_get_pin_level(SW0))
  {
    // Button released
    if (count == DEBOUNCE_COUNT)
    {
      spi_s_async_enable(&SPI_EXT1);
      spi_s_async_enable(&SPI_EXT2);
    }

    count = 0;
  }
  else
  {
    // Button is pressed
    if (count < DEBOUNCE_COUNT)
    {
      count++;
    }
    else if (count == DEBOUNCE_COUNT)
    {
      spi_s_async_disable(&SPI_EXT2);
      spi_s_async_disable(&SPI_EXT1);
    }
  }

  return (count != 0);
}


//---------------------------------------------------------------------------
// Main application
int main(void)
{
  atmel_start_init();

  struct io_descriptor *spi_cmd; // From front panel
  struct io_descriptor *spi_rsp; // From dig-mcu

  spi_s_async_get_io_descriptor(&SPI_EXT1, &spi_cmd);
  spi_s_async_get_io_descriptor(&SPI_EXT2, &spi_rsp);

  spi_s_async_enable(&SPI_EXT1);
  spi_s_async_enable(&SPI_EXT2);

  printf("Front panel monitor\n");
  fflush(stdout);

  bool iscmd = false;
  uint8_t checksum = 0;

  for(;;)
  {
    uint8_t cmdbyte;
    uint8_t rspbyte;

    if (check_button())
    {
      continue;
    }

    // Data should come in on both connections at the same time.
    // So wait until there's data on both ports.
    while (!io_read(spi_cmd, &cmdbyte, 1))
    {
      // Nothing
    }
    while (!io_read(spi_rsp, &rspbyte, 1))
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
    // We also don't handle the case where the first incoming bytes at
    // beginning-of-world are both 0xFF. In that case we simply have no idea
    // who is (not) writing data on the bus but even if we did, we wouldn't
    // know how to decode it anyway so it doesn't matter.
    if (cmdbyte != 0xFF)
    {
      // Data came from front panel
      if (!iscmd)
      {
        iscmd = true;

        gpio_set_pin_level(LED0, false);
        printf("%s\n", (checksum == 0xFF) ? "" : "<!>");

        checksum = 0;
      }
    }
    else if (rspbyte != 0xFF)
    {
      // Data came from dig-mcu
      if (iscmd)
      {
        iscmd = false;

        gpio_set_pin_level(LED0, true);
        printf("%s-- ", (checksum == 0xFF) ? "" : "<!>");

        checksum = 0;
      }
    }
    else
    {
      // Both bytes were 0xFF, don't change direction
    }
    
    uint8_t curbyte = (iscmd ? cmdbyte : rspbyte);

    checksum += curbyte;
    printf("%02X ",  curbyte & 0xFF);
  }  
}
