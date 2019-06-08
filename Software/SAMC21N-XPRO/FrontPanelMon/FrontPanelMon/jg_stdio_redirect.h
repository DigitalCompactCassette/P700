/****************************************************************************
  stdio redirector for SPI
  (C) 2019 Jac Goudsmit
  MIT License

  The stdio redirection facility of Atmel Start doesn't support redirecting
  to a SPI port, only to a synchronous serial port, regardless of how the
  SPI port is configured.

  It's possible to connect it to something else in the graphic tool and
  then use code to redirect it manually (I tested that and it works),
  but even then it won't activate and deactivate the source select line
  for the target that you want to send your debugging information to.

  As it turns out, it's not that hard to redirect stdio yourself by
  overriding the _read() and _write() functions of the C library. The
  overriding functions in this module pull the given SS pin low, then
  send or receive the data to/from the given io descriptor (retrieved at
  startup time and then cached as a static global), and then pulls the SS
  line high again.

  This way, multiple devices can be on the same pins, the way SPI was
  intended, but obviously you have to make sure that you don't call the
  stdio functions from an interrupt function and you don't use the SPI port
  for another device from an interrupt function.
****************************************************************************/


#ifndef JG_STDIO_REDIRECT_INIT_H
#define JG_STDIO_REDIRECT_INIT_H

#ifdef __cplusplus
extern "C"
{
#endif


/////////////////////////////////////////////////////////////////////////////
// FUNCTIONS
/////////////////////////////////////////////////////////////////////////////


//---------------------------------------------------------------------------
// Initialize redirection to a SPI port with a Slave Select pin to activate
void jg_stdio_redirect_init(
  struct spi_m_sync_descriptor * const spi, // SPI master sync host port
  const uint8_t ss_pin);                    // Slave select for target dev


/////////////////////////////////////////////////////////////////////////////
// END
/////////////////////////////////////////////////////////////////////////////


#ifdef __cplusplus
};
#endif

#endif
