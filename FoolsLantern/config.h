/**
 * \file config.h
 *
 * \brief This configuration file is used with the FoolsLantern code to set up special flags
 *
 */
#include <halGpio.h>

#ifndef _CONFIG_H_
#define _CONFIG_H_

// Parameters that affect options in this code:

#define BOARD_VERSION 2			// Indicates which hardware revision to assume
								// NOTE: There is also a parameter in the LED2812.s file that must be changed
//#define FIXED_ADDR true			// Determines whether or not to use a fixed network addr, rather than EEPROM
//#define FIXED_CHAN true			// Determines whether to use a defined channel, or search for best channel

/*****************************************************************************
*****************************************************************************/
// Configuration Options
#ifdef FIXED_ADDR		// If no addr is stored in EEPROM, then this should be defined.
#define APP_ADDR					8				// This is the fixed address that the module will use
#endif
#define APP_CHANNEL					0x0d			// Starts with this channel in the 2.4GHz band
#define LOW_CHANNEL					11			// Lowest channel number
#define HIGH_CHANNEL				26			// Highest channel number
#define NUM_CHANNELS				(HIGH_CHANNEL - LOW_CHANNEL + 1)	// Number of channels
#define APP_PANID					0x4646			// "FF" in ASCII for FestiFools!

#define APP_SECURITY_KEY			"ElectronicLunch0"
#define ADDR_CHECK_INTERVAL			30000		// Interval between checking for duplicate addresses
#define COMMAND_TIMEOUT_INTERVAL	30000			// Number of mS to wait for a command before switching to local mode
#define CHANNEL_SCAN_INTERVAL		2000			// Dwell time on each channel while in local mode
#define LED_ANIMATION_INTERVAL		62				// Number of mS between changes to the LED patternif not static
#define ACCELERATION_INTERVAL		1000			// Interval between increments or decrements of animation rate

#define SYS_SECURITY_MODE                   0

#define NWK_BUFFERS_AMOUNT                  8
#define NWK_MAX_ENDPOINTS_AMOUNT            4
#define NWK_DUPLICATE_REJECTION_TABLE_SIZE  10
#define NWK_DUPLICATE_REJECTION_TTL         2000	// ms
#define NWK_ROUTE_TABLE_SIZE                100		// There are expected to be <100 nodes in the mesh
#define NWK_ROUTE_DEFAULT_SCORE             3
#define NWK_ACK_WAIT_TIME                   500		// ms

#define NWK_ENABLE_ROUTING
//#define NWK_ENABLE_SECURITY						// These flags are used to add in the code for security
//#define PHY_ENABLE_ENERGY_DETECTION				// and for channel energy detection (to avoid busy channels)

#define HAL_ENABLE_UART
#define HAL_UART_CHANNEL                    1
#define HAL_UART_RX_FIFO_SIZE               200
#define HAL_UART_TX_FIFO_SIZE               200

#if BOARD_VERSION == 1								//Board version 1 uses port E for debug and LED outputs
	HAL_GPIO_PIN(hbLED, E, 0)						// The heartbeat flag
	HAL_GPIO_PIN(statusLED, E, 1)					// General status flag
	HAL_GPIO_PIN(rcvLED, E, 2)
	HAL_GPIO_PIN(lightStripData, E, 3)				// This is the serial output to drive the LED strip
#elif BOARD_VERSION == 2							// Board 2 has a connector with pin D4 used for the LED output
	HAL_GPIO_PIN(hbLED, G, 0)						// This is the LED near the antenna on the board
	HAL_GPIO_PIN(statusLED, D, 7)					// This is the LED near the power regulator on the board
	HAL_GPIO_PIN(rcvLED, B, 4)						//
	HAL_GPIO_PIN(lightStripData, D, 4)				// This pin is brought out to the S-LED connector strip
#else
													// There's a problem if this is executed
#endif
	HAL_GPIO_PIN(debug1, E, 4)						// General debugging outputs
	HAL_GPIO_PIN(debug2, E, 5)
	HAL_GPIO_PIN(debug3, E, 6)
	HAL_GPIO_PIN(debug4, E, 7)

	HAL_GPIO_PIN(syncInput, D, 0)					// For the sync

#define NUM_LEDS							16

#endif // _CONFIG_H_
