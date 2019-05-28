/*
 * Code generated from Atmel Start.
 *
 * This file will be overwritten when reconfiguring your Atmel Start project.
 * Please copy examples or other code you want to keep to a separate file
 * to avoid losing it when reconfiguring.
 */

#include "driver_examples.h"
#include "driver_init.h"
#include "utils.h"

/**
 * Example of using SPI_EXT1 to write "Hello World" using the IO abstraction.
 *
 * Since the driver is asynchronous we need to use statically allocated memory for string
 * because driver initiates transfer and then returns before the transmission is completed.
 *
 * Once transfer has been completed the tx_cb function will be called.
 */

static uint8_t example_SPI_EXT1[12] = "Hello World!";

static void complete_cb_SPI_EXT1(const struct spi_s_async_descriptor *const desc)
{
	/* Transfer completed */
}

void SPI_EXT1_example(void)
{
	struct io_descriptor *io;
	spi_s_async_get_io_descriptor(&SPI_EXT1, &io);

	spi_s_async_register_callback(&SPI_EXT1, SPI_S_CB_TX, (FUNC_PTR)complete_cb_SPI_EXT1);
	spi_s_async_enable(&SPI_EXT1);
	io_write(io, example_SPI_EXT1, 12);
}

/**
 * Example of using EDBG_COM to write "Hello World" using the IO abstraction.
 */
void EDBG_COM_example(void)
{
	struct io_descriptor *io;
	usart_sync_get_io_descriptor(&EDBG_COM, &io);
	usart_sync_enable(&EDBG_COM);

	io_write(io, (uint8_t *)"Hello World!", 12);
}
