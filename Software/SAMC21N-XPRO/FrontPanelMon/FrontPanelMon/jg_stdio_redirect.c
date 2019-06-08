/****************************************************************************
  stdio redirector for SPI
  (C) 2019 Jac Goudsmit
  MIT License.

  Based on the stdio redirector in Atmel Start, but this uses a SPI device
  and pulls the select-line for the given device low while it's sending and
  receiving data. Receiving has not been tested.
****************************************************************************/


/////////////////////////////////////////////////////////////////////////////
// INCLUDES
/////////////////////////////////////////////////////////////////////////////


#include <atmel_start.h>
#include <atmel_start_pins.h>
#include <stdio.h>
#include <assert.h>

#include "jg_stdio_redirect.h"


/////////////////////////////////////////////////////////////////////////////
// STATIC DATA
/////////////////////////////////////////////////////////////////////////////


static struct io_descriptor *jg_stdio_io;
static uint8_t jg_stdio_ss_pin;


/////////////////////////////////////////////////////////////////////////////
// PUBLIC FUNCTIONS
/////////////////////////////////////////////////////////////////////////////


//---------------------------------------------------------------------------
// Override for the _read function of the stdio library
int __attribute__((weak)) _read(int file, char *ptr, int len); /* Remove GCC compiler warning */
int __attribute__((weak)) _read(int file, char *ptr, int len)
{
  int n = 0;

  if ((!jg_stdio_io) || (file != 0))
  {
    return -1;
  }

  gpio_set_pin_level(SPI_EDBG_SS, false);
  n = io_read(jg_stdio_io, (uint8_t *)ptr, len);
  gpio_set_pin_level(SPI_EDBG_SS, true);
  if (n < 0)
  {
    return -1;
  }

  return n;
}


//---------------------------------------------------------------------------
// Override for the _write function of the stdio library 
int __attribute__((weak)) _write(int file, char *ptr, int len); /* Remove GCC compiler warning */
int __attribute__((weak)) _write(int file, char *ptr, int len)
{
  int n = 0;

  if ((!jg_stdio_io) || ((file != 1) && (file != 2) && (file != 3)))
  {
    return -1;
  }

  gpio_set_pin_level(SPI_EDBG_SS, false);
  n = io_write(jg_stdio_io, (const uint8_t *)ptr, len);
  gpio_set_pin_level(SPI_EDBG_SS, true);
  if (n < 0)
  {
    return -1;
  }

  return n;
}


//---------------------------------------------------------------------------
// Initialize redirection to a SPI port with a Slave Select pin to activate
void jg_stdio_redirect_init(struct spi_m_sync_descriptor * const spi, const uint8_t ss_pin)
{
  assert(spi);

#if defined(__GNUC__)
  /* Specify that stdout and stdin should not be buffered. */
  setbuf(stdout, NULL);
  setbuf(stdin, NULL);
  /* Note: Already the case in IAR's Normal DLIB default configuration
   * and AVR GCC library:
   * - printf() emits one character at a time.
   * - getchar() requests only 1 byte to exit.
   */
#endif

  spi_m_sync_get_io_descriptor(spi, &jg_stdio_io);
  jg_stdio_ss_pin = ss_pin;

  spi_m_sync_enable(spi);

  // Force CS sync to EDBG by enabling the slave pin twice
  gpio_set_pin_level(jg_stdio_ss_pin, true);
  gpio_set_pin_level(jg_stdio_ss_pin, false);
  gpio_set_pin_level(jg_stdio_ss_pin, true);
  gpio_set_pin_level(jg_stdio_ss_pin, false);
}


/////////////////////////////////////////////////////////////////////////////
// END
/////////////////////////////////////////////////////////////////////////////