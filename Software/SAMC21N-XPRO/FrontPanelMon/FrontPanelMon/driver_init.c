/*
 * Code generated from Atmel Start.
 *
 * This file will be overwritten when reconfiguring your Atmel Start project.
 * Please copy examples or other code you want to keep to a separate file
 * to avoid losing it when reconfiguring.
 */

#include "driver_init.h"
#include <peripheral_clk_config.h>
#include <utils.h>
#include <hal_init.h>

struct spi_s_async_descriptor SPI_EXT1;
static uint16_t               SPI_EXT1_buf[16];

struct usart_sync_descriptor EDBG_COM;

struct spi_s_async_descriptor SPI_EXT2;
static uint16_t               SPI_EXT2_buf[16];

void SPI_EXT1_PORT_init(void)
{

	gpio_set_pin_level(PC27,
	                   // <y> Initial level
	                   // <id> pad_initial_level
	                   // <false"> Low
	                   // <true"> High
	                   false);

	// Set pin direction to output
	gpio_set_pin_direction(PC27, GPIO_DIRECTION_OUT);

	gpio_set_pin_function(PC27, PINMUX_PC27D_SERCOM1_PAD0);

	// Set pin direction to input
	gpio_set_pin_direction(PC28, GPIO_DIRECTION_IN);

	gpio_set_pin_pull_mode(PC28,
	                       // <y> Pull configuration
	                       // <id> pad_pull_config
	                       // <GPIO_PULL_OFF"> Off
	                       // <GPIO_PULL_UP"> Pull-up
	                       // <GPIO_PULL_DOWN"> Pull-down
	                       GPIO_PULL_OFF);

	gpio_set_pin_function(PC28, PINMUX_PC28D_SERCOM1_PAD1);

	// Set pin direction to input
	gpio_set_pin_direction(PA18, GPIO_DIRECTION_IN);

	gpio_set_pin_pull_mode(PA18,
	                       // <y> Pull configuration
	                       // <id> pad_pull_config
	                       // <GPIO_PULL_OFF"> Off
	                       // <GPIO_PULL_UP"> Pull-up
	                       // <GPIO_PULL_DOWN"> Pull-down
	                       GPIO_PULL_OFF);

	gpio_set_pin_function(PA18, PINMUX_PA18C_SERCOM1_PAD2);

	gpio_set_pin_level(PA19,
	                   // <y> Initial level
	                   // <id> pad_initial_level
	                   // <false"> Low
	                   // <true"> High
	                   false);

	// Set pin direction to output
	gpio_set_pin_direction(PA19, GPIO_DIRECTION_OUT);

	gpio_set_pin_function(PA19, PINMUX_PA19C_SERCOM1_PAD3);
}

void SPI_EXT1_CLOCK_init(void)
{
	hri_gclk_write_PCHCTRL_reg(GCLK, SERCOM1_GCLK_ID_CORE, CONF_GCLK_SERCOM1_CORE_SRC | (1 << GCLK_PCHCTRL_CHEN_Pos));
	hri_gclk_write_PCHCTRL_reg(GCLK, SERCOM1_GCLK_ID_SLOW, CONF_GCLK_SERCOM1_SLOW_SRC | (1 << GCLK_PCHCTRL_CHEN_Pos));

	hri_mclk_set_APBCMASK_SERCOM1_bit(MCLK);
}

void SPI_EXT1_init(void)
{
	SPI_EXT1_CLOCK_init();
	spi_s_async_init(&SPI_EXT1, SERCOM1, (uint8_t *)SPI_EXT1_buf, 32);
	SPI_EXT1_PORT_init();
}

void EDBG_COM_PORT_init(void)
{

	gpio_set_pin_function(PB10, PINMUX_PB10D_SERCOM4_PAD2);

	gpio_set_pin_function(PB11, PINMUX_PB11D_SERCOM4_PAD3);
}

void EDBG_COM_CLOCK_init(void)
{
	hri_gclk_write_PCHCTRL_reg(GCLK, SERCOM4_GCLK_ID_CORE, CONF_GCLK_SERCOM4_CORE_SRC | (1 << GCLK_PCHCTRL_CHEN_Pos));
	hri_gclk_write_PCHCTRL_reg(GCLK, SERCOM4_GCLK_ID_SLOW, CONF_GCLK_SERCOM4_SLOW_SRC | (1 << GCLK_PCHCTRL_CHEN_Pos));

	hri_mclk_set_APBCMASK_SERCOM4_bit(MCLK);
}

void EDBG_COM_init(void)
{
	EDBG_COM_CLOCK_init();
	usart_sync_init(&EDBG_COM, SERCOM4, (void *)NULL);
	EDBG_COM_PORT_init();
}

void SPI_EXT2_PORT_init(void)
{

	gpio_set_pin_level(PB02,
	                   // <y> Initial level
	                   // <id> pad_initial_level
	                   // <false"> Low
	                   // <true"> High
	                   false);

	// Set pin direction to output
	gpio_set_pin_direction(PB02, GPIO_DIRECTION_OUT);

	gpio_set_pin_function(PB02, PINMUX_PB02D_SERCOM5_PAD0);

	// Set pin direction to input
	gpio_set_pin_direction(PB03, GPIO_DIRECTION_IN);

	gpio_set_pin_pull_mode(PB03,
	                       // <y> Pull configuration
	                       // <id> pad_pull_config
	                       // <GPIO_PULL_OFF"> Off
	                       // <GPIO_PULL_UP"> Pull-up
	                       // <GPIO_PULL_DOWN"> Pull-down
	                       GPIO_PULL_OFF);

	gpio_set_pin_function(PB03, PINMUX_PB03D_SERCOM5_PAD1);

	// Set pin direction to input
	gpio_set_pin_direction(PB00, GPIO_DIRECTION_IN);

	gpio_set_pin_pull_mode(PB00,
	                       // <y> Pull configuration
	                       // <id> pad_pull_config
	                       // <GPIO_PULL_OFF"> Off
	                       // <GPIO_PULL_UP"> Pull-up
	                       // <GPIO_PULL_DOWN"> Pull-down
	                       GPIO_PULL_OFF);

	gpio_set_pin_function(PB00, PINMUX_PB00D_SERCOM5_PAD2);

	gpio_set_pin_level(PB01,
	                   // <y> Initial level
	                   // <id> pad_initial_level
	                   // <false"> Low
	                   // <true"> High
	                   false);

	// Set pin direction to output
	gpio_set_pin_direction(PB01, GPIO_DIRECTION_OUT);

	gpio_set_pin_function(PB01, PINMUX_PB01D_SERCOM5_PAD3);
}

void SPI_EXT2_CLOCK_init(void)
{
	hri_gclk_write_PCHCTRL_reg(GCLK, SERCOM5_GCLK_ID_CORE, CONF_GCLK_SERCOM5_CORE_SRC | (1 << GCLK_PCHCTRL_CHEN_Pos));
	hri_gclk_write_PCHCTRL_reg(GCLK, SERCOM5_GCLK_ID_SLOW, CONF_GCLK_SERCOM5_SLOW_SRC | (1 << GCLK_PCHCTRL_CHEN_Pos));

	hri_mclk_set_APBCMASK_SERCOM5_bit(MCLK);
}

void SPI_EXT2_init(void)
{
	SPI_EXT2_CLOCK_init();
	spi_s_async_init(&SPI_EXT2, SERCOM5, (uint8_t *)SPI_EXT2_buf, 32);
	SPI_EXT2_PORT_init();
}

void system_init(void)
{
	init_mcu();

	// GPIO on PB19

	// Set pin direction to input
	gpio_set_pin_direction(SW0, GPIO_DIRECTION_IN);

	gpio_set_pin_pull_mode(SW0,
	                       // <y> Pull configuration
	                       // <id> pad_pull_config
	                       // <GPIO_PULL_OFF"> Off
	                       // <GPIO_PULL_UP"> Pull-up
	                       // <GPIO_PULL_DOWN"> Pull-down
	                       GPIO_PULL_UP);

	gpio_set_pin_function(SW0, GPIO_PIN_FUNCTION_OFF);

	// GPIO on PC05

	gpio_set_pin_level(LED0,
	                   // <y> Initial level
	                   // <id> pad_initial_level
	                   // <false"> Low
	                   // <true"> High
	                   false);

	// Set pin direction to output
	gpio_set_pin_direction(LED0, GPIO_DIRECTION_OUT);

	gpio_set_pin_function(LED0, GPIO_PIN_FUNCTION_OFF);

	SPI_EXT1_init();

	EDBG_COM_init();

	SPI_EXT2_init();
}
