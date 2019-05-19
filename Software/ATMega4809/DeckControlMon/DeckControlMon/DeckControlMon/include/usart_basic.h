/**
 * \file
 *
 * \brief USART basic driver.
 *
 (c) 2018 Microchip Technology Inc. and its subsidiaries.

    Subject to your compliance with these terms,you may use this software and
    any derivatives exclusively with Microchip products.It is your responsibility
    to comply with third party license terms applicable to your use of third party
    software (including open source software) that may accompany Microchip software.

    THIS SOFTWARE IS SUPPLIED BY MICROCHIP "AS IS". NO WARRANTIES, WHETHER
    EXPRESS, IMPLIED OR STATUTORY, APPLY TO THIS SOFTWARE, INCLUDING ANY IMPLIED
    WARRANTIES OF NON-INFRINGEMENT, MERCHANTABILITY, AND FITNESS FOR A
    PARTICULAR PURPOSE.

    IN NO EVENT WILL MICROCHIP BE LIABLE FOR ANY INDIRECT, SPECIAL, PUNITIVE,
    INCIDENTAL OR CONSEQUENTIAL LOSS, DAMAGE, COST OR EXPENSE OF ANY KIND
    WHATSOEVER RELATED TO THE SOFTWARE, HOWEVER CAUSED, EVEN IF MICROCHIP HAS
    BEEN ADVISED OF THE POSSIBILITY OR THE DAMAGES ARE FORESEEABLE. TO THE
    FULLEST EXTENT ALLOWED BY LAW, MICROCHIP'S TOTAL LIABILITY ON ALL CLAIMS IN
    ANY WAY RELATED TO THIS SOFTWARE WILL NOT EXCEED THE AMOUNT OF FEES, IF ANY,
    THAT YOU HAVE PAID DIRECTLY TO MICROCHIP FOR THIS SOFTWARE.
 *
 */

#ifndef USART_BASIC_H_INCLUDED
#define USART_BASIC_H_INCLUDED

#include <atmel_start.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Normal Mode, Baud register value */
#define USART0_BAUD_RATE(BAUD_RATE) ((float)(3333333.33333 * 64 / (16 * (float)BAUD_RATE)) + 0.5)

int8_t USARTPA1_init();

void USARTPA1_enable();

void USARTPA1_enable_rx();

void USARTPA1_enable_tx();

void USARTPA1_disable();

uint8_t USARTPA1_get_data();

bool USARTPA1_is_tx_ready();

bool USARTPA1_is_rx_ready();

bool USARTPA1_is_tx_busy();

uint8_t USARTPA1_read(void);

void USARTPA1_write(const uint8_t data);

/* Normal Mode, Baud register value */
#define USART1_BAUD_RATE(BAUD_RATE) ((float)(3333333.33333 * 64 / (16 * (float)BAUD_RATE)) + 0.5)

int8_t USARTPC1_init();

void USARTPC1_enable();

void USARTPC1_enable_rx();

void USARTPC1_enable_tx();

void USARTPC1_disable();

uint8_t USARTPC1_get_data();

bool USARTPC1_is_tx_ready();

bool USARTPC1_is_rx_ready();

bool USARTPC1_is_tx_busy();

uint8_t USARTPC1_read(void);

void USARTPC1_write(const uint8_t data);

/* Normal Mode, Baud register value */
#define USART3_BAUD_RATE(BAUD_RATE) ((float)(3333333.33333 * 64 / (16 * (float)BAUD_RATE)) + 0.5)

int8_t USBSER_init();

void USBSER_enable();

void USBSER_enable_rx();

void USBSER_enable_tx();

void USBSER_disable();

uint8_t USBSER_get_data();

bool USBSER_is_tx_ready();

bool USBSER_is_rx_ready();

bool USBSER_is_tx_busy();

uint8_t USBSER_read(void);

void USBSER_write(const uint8_t data);

#ifdef __cplusplus
}
#endif

#endif /* USART_BASIC_H_INCLUDED */
