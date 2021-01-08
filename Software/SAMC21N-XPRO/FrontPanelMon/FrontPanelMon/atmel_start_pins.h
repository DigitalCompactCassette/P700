/*
 * Code generated from Atmel Start.
 *
 * This file will be overwritten when reconfiguring your Atmel Start project.
 * Please copy examples or other code you want to keep to a separate file
 * to avoid losing it when reconfiguring.
 */
#ifndef ATMEL_START_PINS_H_INCLUDED
#define ATMEL_START_PINS_H_INCLUDED

#include <hal_gpio.h>

// SAMC21 has 9 pin functions

#define GPIO_PIN_FUNCTION_A 0
#define GPIO_PIN_FUNCTION_B 1
#define GPIO_PIN_FUNCTION_C 2
#define GPIO_PIN_FUNCTION_D 3
#define GPIO_PIN_FUNCTION_E 4
#define GPIO_PIN_FUNCTION_F 5
#define GPIO_PIN_FUNCTION_G 6
#define GPIO_PIN_FUNCTION_H 7
#define GPIO_PIN_FUNCTION_I 8

#define PA18 GPIO(GPIO_PORTA, 18)
#define PA19 GPIO(GPIO_PORTA, 19)
#define DGI_GPIO1 GPIO(GPIO_PORTA, 22)
#define PB00 GPIO(GPIO_PORTB, 0)
#define PB01 GPIO(GPIO_PORTB, 1)
#define PB02 GPIO(GPIO_PORTB, 2)
#define PB03 GPIO(GPIO_PORTB, 3)
#define PB10 GPIO(GPIO_PORTB, 10)
#define PB11 GPIO(GPIO_PORTB, 11)
#define DGI_GPIO0 GPIO(GPIO_PORTB, 12)
#define SW0 GPIO(GPIO_PORTB, 19)
#define LED0 GPIO(GPIO_PORTC, 5)
#define SPI_EDBG_SS GPIO(GPIO_PORTC, 9)
#define PC27 GPIO(GPIO_PORTC, 27)
#define PC28 GPIO(GPIO_PORTC, 28)

#endif // ATMEL_START_PINS_H_INCLUDED
