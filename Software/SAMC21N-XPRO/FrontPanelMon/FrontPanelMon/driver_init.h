/*
 * Code generated from Atmel Start.
 *
 * This file will be overwritten when reconfiguring your Atmel Start project.
 * Please copy examples or other code you want to keep to a separate file
 * to avoid losing it when reconfiguring.
 */
#ifndef DRIVER_INIT_INCLUDED
#define DRIVER_INIT_INCLUDED

#include "atmel_start_pins.h"

#ifdef __cplusplus
extern "C" {
#endif

#include <hal_atomic.h>
#include <hal_delay.h>
#include <hal_gpio.h>
#include <hal_init.h>
#include <hal_io.h>
#include <hal_sleep.h>

#include <hal_ext_irq.h>

#include <hal_spi_s_async.h>

#include <hal_usart_sync.h>

#include <hal_spi_s_async.h>

#include <hal_spi_s_async.h>

extern struct spi_s_async_descriptor SPI_EXT1;

extern struct usart_sync_descriptor SER_EDBG;

extern struct spi_s_async_descriptor SPI_EXT2;

extern struct spi_s_async_descriptor SPI_EXT3;

void SPI_EXT1_PORT_init(void);
void SPI_EXT1_CLOCK_init(void);
void SPI_EXT1_init(void);

void SER_EDBG_PORT_init(void);
void SER_EDBG_CLOCK_init(void);
void SER_EDBG_init(void);

void SPI_EXT2_PORT_init(void);
void SPI_EXT2_CLOCK_init(void);
void SPI_EXT2_init(void);

void SPI_EXT3_PORT_init(void);
void SPI_EXT3_CLOCK_init(void);
void SPI_EXT3_init(void);

/**
 * \brief Perform system initialization, initialize pins and clocks for
 * peripherals
 */
void system_init(void);

#ifdef __cplusplus
}
#endif
#endif // DRIVER_INIT_INCLUDED
