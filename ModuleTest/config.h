/*
 * \file config.h
 *
 * \brief Template application and stack configuration
 *
 *
 */
#include <halGpio.h>

#ifndef _CONFIG_H_
#define _CONFIG_H_

// Parameters that affect options in this code:

//#define FIXED_ADDR true		// Determines whether or not to use a fixed network addr, rather than EEPROM
#define BOARD_VERSION 2			// Indicates which hardware revision to assume

/*****************************************************************************
*****************************************************************************/
// Configuration Options
#ifdef FIXED_ADDR
	#define APP_ADDR                  8
#endif
#define APP_CHANNEL               0x0f
#define APP_PANID                 0x4646		// FF in ASCII

#define APP_SECURITY_KEY          "TestSecurityKey0"
#define APP_FLUSH_TIMER_INTERVAL  250

#define SYS_SECURITY_MODE                   0

#define NWK_BUFFERS_AMOUNT                  8
#define NWK_MAX_ENDPOINTS_AMOUNT            4
#define NWK_DUPLICATE_REJECTION_TABLE_SIZE  10
#define NWK_DUPLICATE_REJECTION_TTL         2000 // ms
#define NWK_ROUTE_TABLE_SIZE                100
#define NWK_ROUTE_DEFAULT_SCORE             3
#define NWK_ACK_WAIT_TIME                   500 // ms

#define NWK_ENABLE_ROUTING
//#define NWK_ENABLE_SECURITY
#define PHY_ENABLE_ENERGY_DETECTION

#define HAL_ENABLE_UART
#define HAL_UART_CHANNEL                    1
#define HAL_UART_RX_FIFO_SIZE               200
#define HAL_UART_TX_FIFO_SIZE               200

#if BOARD_VERSION == 1
	HAL_GPIO_PIN(hbLED, E, 0)
	HAL_GPIO_PIN(statusLED, E, 1)
	HAL_GPIO_PIN(rcvLED, E, 2)
	HAL_GPIO_PIN(LEDDataOut, E, 8)
	HAL_GPIO_PIN(lightStripData, E, 3)
	HAL_GPIO_PIN(debug1, E, 4)
	HAL_GPIO_PIN(debug2, E, 5)
	HAL_GPIO_PIN(debug3, E, 6)
	HAL_GPIO_PIN(debug4, E, 7)
#elif BOARD_VERSION == 2
	HAL_GPIO_PIN(hbLED, G, 0)
	HAL_GPIO_PIN(statusLED, D, 7)
	HAL_GPIO_PIN(rcvLED, B, 4)
	HAL_GPIO_PIN(lightStripData, D, 4)
	HAL_GPIO_PIN(debug1, E, 4)
	HAL_GPIO_PIN(debug2, E, 5)
	HAL_GPIO_PIN(debug3, E, 6)
	HAL_GPIO_PIN(debug4, E, 7)
#else
#endif

#define NUM_LEDS							16

#endif // _CONFIG_H_
