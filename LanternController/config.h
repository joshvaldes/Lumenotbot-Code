/**
 * \file config.h
 *
 * \brief Template application and stack configuration
 *
 * Copyright (C) 2012 Atmel Corporation. All rights reserved.
 *
 * \asf_license_start
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * 3. The name of Atmel may not be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * 4. This software may only be redistributed and used in connection with an
 *    Atmel microcontroller product.
 *
 * THIS SOFTWARE IS PROVIDED BY ATMEL "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT ARE
 * EXPRESSLY AND SPECIFICALLY DISCLAIMED. IN NO EVENT SHALL ATMEL BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * \asf_license_stop
 *
 * $Id: config.h 5223 2012-09-10 16:47:17Z ataradov $
 *
 */
#include <halGpio.h>

#ifndef _CONFIG_H_
#define _CONFIG_H_

// Parameters that affect options in this code:

#define BOARD_VERSION 2			// Indicates which hardware revision to assume
//#define FIXED_CHAN true			// Determines whether to use a defined channel, or search for best channel
#define FIXED_ADDR true		// Determines whether or not to use a fixed network addr, rather than EEPROM
#define FREERUN
/*****************************************************************************
*****************************************************************************/
// Configuration Options

#ifdef FIXED_ADDR
#define APP_ADDR					1
#endif
#define APP_CHANNEL					0x0d
#define LOW_CHANNEL					11			// Lowest channel number
#define HIGH_CHANNEL				26			// Highest channel number
#define NUM_CHANNELS				(HIGH_CHANNEL - LOW_CHANNEL + 1)	// Number of channels
#define APP_PANID					0x4646		// FF in ASCII
#define APP_ENDPOINT				1
#define APP_SECURITY_KEY			"ElectronicLunch0"
#define APP_SEND_TIMER_INTERVAL		500
#define IO_POLL_TIMER_INTERVAL		250
#define COMMAND_TIMEOUT_INTERVAL	25000			// Number of mS between command updates to avoid timeout

#define SYS_SECURITY_MODE                   0

#define NWK_BUFFERS_AMOUNT                  3
#define NWK_MAX_ENDPOINTS_AMOUNT            4
#define NWK_DUPLICATE_REJECTION_TABLE_SIZE  10
#define NWK_DUPLICATE_REJECTION_TTL         2000 // ms
#define NWK_ROUTE_TABLE_SIZE                100
#define NWK_ROUTE_DEFAULT_SCORE             3
#define NWK_ACK_WAIT_TIME                   500 // ms

#define NWK_ENABLE_ROUTING
#define NWK_ENABLE_SECURITY
#define PHY_ENABLE_ENERGY_DETECTION

//#define HAL_ENABLE_UART
#define HAL_UART_CHANNEL                    1
#define HAL_UART_RX_FIFO_SIZE               200
#define HAL_UART_TX_FIFO_SIZE               200

#if BOARD_VERSION == 1
	HAL_GPIO_PIN(hbLED, E, 0)
	HAL_GPIO_PIN(statusLED, E, 1)
	HAL_GPIO_PIN(sendStatusLED, E, 2)
#elif BOARD_VERSION == 2
	HAL_GPIO_PIN(hbLED, G, 0)
	HAL_GPIO_PIN(statusLED, D, 7)
	HAL_GPIO_PIN(sendStatusLED, E, 0)
#else
#endif
	HAL_GPIO_PIN(leftBttn, E, 6)
	HAL_GPIO_PIN(rightBttn, E, 7)
	HAL_GPIO_PIN(potPullup, F, 3)

#define redChannel 2
#define greenChannel 1
#define blueChannel 0

#define NUM_LEDS							16

#endif // _CONFIG_H_
