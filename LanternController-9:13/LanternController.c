/**
* \file LanternController.c
*
* \brief The master application that sends commands into the mesh to LED nodes.
*
* Based on samples Copyright (C) 2012 Atmel Corporation. All rights reserved.
*
*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <avr/eeprom.h>
#include <util/atomic.h>
#include "config.h"
#include "sys.h"
#include "phy.h"
#include "nwk.h"
#include "sysTimer.h"
#include "FoolsModes.h"

/*****************************************************************************
Preprocessor definitions
*****************************************************************************/

#ifdef NWK_ENABLE_SECURITY
#define APP_BUFFER_SIZE     NWK_MAX_SECURED_PAYLOAD_SIZE
#else
#define APP_BUFFER_SIZE     NWK_MAX_PAYLOAD_SIZE
#endif
/*****************************************************************************
Type definitions
*****************************************************************************/

typedef enum AppState_t
{
	APP_STATE_INITIAL,
	APP_STATE_IDLE,
	APP_STATE_WAITSEND,
	APP_STATE_RECD,
	APP_STATE_CHANNELSCAN
} AppState_t;

typedef enum LED_Mode_t
{
	LED_MODE_IDLE,
	LED_MODE_FIXED,
	LED_MODE_RANDOM,
	LED_MODE_FLASHING,
	LED_MODE_THROBBING
} LED_Mode_t;

/*****************************************************************************
Function prototypes
*****************************************************************************/
// Provided by LPW stack
static void appSendData(void);
// provided by Roger S
extern void InitADC (void);
extern uint8_t GetADC (uint8_t channel);
extern void updateLEDs (uint8_t colorArray[], uint16_t numLEDs);
extern void outPortE (uint8_t diagInfo);

/*****************************************************************************
Variables
*****************************************************************************/

static uint16_t myAddr;

// This is used to switch between a node address stored in EEPROM and one provided in the
// configuration file.
#ifndef FIXED_ADDR
static uint16_t APP_ADDR EEMEM;
#endif
static AppState_t appState = APP_STATE_INITIAL;
static SYS_Timer_t sendCmdTimer;
static SYS_Timer_t pollInputsTimer;
static SYS_Timer_t meshHeartbeatTimer;
#ifdef PHY_ENABLE_ENERGY_DETECTION
static SYS_Timer_t channelScanTimer;
#endif
static NWK_DataReq_t appDataReq;
static bool appDataReqBusy = false;
static uint8_t appDataReqBuffer[APP_BUFFER_SIZE];
static uint8_t appWorkingBuffer[APP_BUFFER_SIZE];
static uint8_t appWorkingBufferLen = 0;
static uint8_t appWorkingBufferPtr = 0;

static LED_Command_t *cmdBuffer;
static uint8_t currentLEDmode;
static uint8_t demoCounter;
static uint8_t shotCounter;

#ifdef PHY_ENABLE_ENERGY_DETECTION
static uint8_t chanEnergy[HIGH_CHANNEL+1];
static uint8_t energyByte;
static uint8_t currentChannel;
static bool channelComplete;
static uint8_t bestChannel;
static uint8_t LEDarray[NUM_LEDS*3];
#endif

static uint8_t redADC;
static uint8_t grnADC;
static uint8_t bluADC;
static uint8_t leftButton;
static uint8_t rightButton;
static bool readLeft;
static uint8_t buttonMode;

static uint16_t mainLoopBlink;
static uint16_t targetAddr;

/*****************************************************************************
Function implementations
*****************************************************************************/

/*****************************************************************************
// The callback function that the network stack uses to tell the application that
// the last request has been processed.  Used here to allow the next send to
// proceed.
*****************************************************************************/
static void appDataConf(NWK_DataReq_t *req)
{
	#ifdef BLINK_LEDS
	if ((req->status == NWK_SUCCESS_STATUS)||(req->status == NWK_NO_ACK_STATUS)||(req->status == NWK_PHY_NO_ACK_STATUS)) {
		HAL_GPIO_sendStatusLED_set();
	} else {
		HAL_GPIO_sendStatusLED_clr();
	}
	#endif
	appDataReqBusy = false;
}

/*****************************************************************************
// The function used to send data to other nodes in the mesh.
*****************************************************************************/
static void appSendData(void)
{
	if (appDataReqBusy)
	return;

	memcpy(appDataReqBuffer, appWorkingBuffer, sizeof(LED_Command_t));

	appDataReq.dstAddr = targetAddr;
	appDataReq.dstEndpoint = LEDCmd_ENDPOINT;
	appDataReq.srcEndpoint = LEDCmd_ENDPOINT;
	if (targetAddr == BROADCAST_ADDR)
	{
		#ifdef NWK_ENABLE_SECURITY
		appDataReq.options = NWK_OPT_ENABLE_SECURITY;
		#endif
	} else
	{
		#ifdef NWK_ENABLE_SECURITY
		appDataReq.options = NWK_OPT_ACK_REQUEST | NWK_OPT_ENABLE_SECURITY;
		#else
		appDataReq.options = NWK_OPT_ACK_REQUEST;
		#endif
	}
	appDataReq.data = appDataReqBuffer;
	appDataReq.size = sizeof(LED_Command_t);
	appDataReq.confirm = appDataConf;
	NWK_DataReq(&appDataReq);
	#ifdef BLINK_LEDS
	HAL_GPIO_statusLED_toggle();
	HAL_GPIO_sendStatusLED_clr();
	#endif
	appWorkingBufferPtr = 0;
	appDataReqBusy = true;

	targetAddr = BROADCAST_ADDR;
}

/*****************************************************************************
This function checks the analog and digital inputs on a periodic basis.  The
interval is set in the config.h file as IO_POLL_TIMER_INTERVAL.
*****************************************************************************/
static void pollIOTimerHandler(SYS_Timer_t *timer)
{
	leftButton = HAL_GPIO_leftBttn_read();
	rightButton = HAL_GPIO_rightBttn_read();
	if(leftButton && readLeft)				// Button is active LOW
	{
		readLeft = false;
		buttonMode++;
		shotCounter = 1;
	} else if (rightButton && !readLeft)	// Button is active LOW
	{
		readLeft = true;
		buttonMode++;
		shotCounter = 1;
	}
	if (buttonMode > ONESHOT)
	{
		buttonMode = STATIC;
	}
	redADC = GetADC(redChannel);
	grnADC = GetADC(greenChannel);
	bluADC = GetADC(blueChannel);
}

/*****************************************************************************
The lanterns have a built-in timeout so that if no commands are received
within a certain interval, then they switch to local or mesh mode.  This
timer function ensures that a command is sent out periodically so that
the lanterns never drop out to local mode.
*****************************************************************************/
static void meshHeartbeatTimerHandler(SYS_Timer_t *timer)
{
	// This is just an empty message to avoid command timeouts in the lanterns
	cmdBuffer = appWorkingBuffer;
	cmdBuffer->mode = MODE_NOCHANGE;
	appWorkingBufferPtr = 1;
	appSendData();
}

/*****************************************************************************
This is a callback function that is triggered when the periodic timer says
it's time to send out the next command to the mesh.
There are two distinct modes, set by a config.h parameter.  In FREERUN mode
it runs in a demo mode, switching through each of the modes with fixed
parameters.  In normal mode, it takes inputs from the switches and voltage
divider inputs and uses those to set both mode and color patterns.
*****************************************************************************/

static void sendCmdTimerHandler(SYS_Timer_t *timer)
{
	cmdBuffer = appWorkingBuffer;
	cmdBuffer->mode = MODE_GLOBAL;
	#ifdef FREERUN
	if (demoCounter <= 5)
	{
		if (demoCounter == 1)
		{
			shotCounter = 1;
		}
		#else
		if (buttonMode == ONESHOT)
		{
			#endif
			cmdBuffer->subMode = ONESHOT;
			cmdBuffer->period_mS = 250;
			for (int LED_ptr=0;LED_ptr<NUM_LEDS;LED_ptr++)
			{
				#ifdef FREERUN
				cmdBuffer->redIntensity[LED_ptr] = 0xFF;				// Green
				cmdBuffer->grnIntensity[LED_ptr] = 0xFF;				// Red
				cmdBuffer->bluIntensity[LED_ptr] = 0xFF;				// Blue
				#else
				cmdBuffer->redIntensity[LED_ptr] = redADC;				// Red
				cmdBuffer->grnIntensity[LED_ptr] = grnADC;				// Green
				cmdBuffer->bluIntensity[LED_ptr] = bluADC;				// Blue
				#endif
			}
			#ifdef FREERUN
		} else if (demoCounter < 25)
		{
			if (demoCounter < 5)
			{
				shotCounter = 20;
			}
			#else
		} else if (buttonMode == STATIC)
		{
			shotCounter = 2;
			#endif
			cmdBuffer->subMode = STATIC;
			cmdBuffer->period_mS = 250;
			for (int LED_ptr=0;LED_ptr<NUM_LEDS;LED_ptr++)
			{
				#ifdef FREERUN
				cmdBuffer->redIntensity[LED_ptr] = 0xFF;				// Green
				cmdBuffer->grnIntensity[LED_ptr] = 0xFF;				// Red
				cmdBuffer->bluIntensity[LED_ptr] = 0xFF;				// Blue
				#else
				cmdBuffer->redIntensity[LED_ptr] = redADC;				// Red
				cmdBuffer->grnIntensity[LED_ptr] = grnADC;				// Green
				cmdBuffer->bluIntensity[LED_ptr] = bluADC;				// Blue
				#endif
			}
			#ifdef FREERUN
		} else if (demoCounter < 45)
		{
			if (demoCounter == 10)
			{
				shotCounter = 1;
			}
			#else
		} else if (buttonMode == ROTATE)
		{
			#endif
			cmdBuffer->subMode = ROTATE;
			cmdBuffer->period_mS = 62;
			for (int LED_ptr=0;LED_ptr<NUM_LEDS;LED_ptr++)
			{
				#ifdef FREERUN
				cmdBuffer->redIntensity[LED_ptr] = LED_ptr*16-1;
				cmdBuffer->grnIntensity[LED_ptr] = 0;
				cmdBuffer->bluIntensity[LED_ptr] = 0;
				#else
				// Fill the LED buffer with a fading pattern according to position and ADC input values
				if (redADC > (LED_ptr*16))
				{
					cmdBuffer->redIntensity[LED_ptr] = redADC-(LED_ptr*16);				// Red
				} else {
					cmdBuffer->redIntensity[LED_ptr] = 0;
				}
				if (grnADC > (LED_ptr*16))
				{
					cmdBuffer->grnIntensity[LED_ptr] = grnADC-(LED_ptr*16);				// Green
				} else {
					cmdBuffer->grnIntensity[LED_ptr] = 0;
				}
				if (bluADC > (LED_ptr*16))
				{
					cmdBuffer->bluIntensity[LED_ptr] = bluADC-(LED_ptr*16);				// Blue
				} else {
					cmdBuffer->bluIntensity[LED_ptr] = 0;
				}
				#endif
			}
			#ifdef FREERUN
		} else if (demoCounter < 20)
		{
			if (demoCounter == 15)
			{
				shotCounter = 1;
			}
			#else
		} else if (buttonMode == FLASH)
		{
			#endif
			cmdBuffer->subMode = FLASH;
			cmdBuffer->period_mS = 250;
			for (int LED_ptr=0;LED_ptr<NUM_LEDS;LED_ptr++)
			{
				#ifdef FREERUN
				cmdBuffer->redIntensity[LED_ptr] = 0xFF;				// Green
				cmdBuffer->grnIntensity[LED_ptr] = 0xFF;				// Red
				cmdBuffer->bluIntensity[LED_ptr] = 0xFF;				// Blue
				#else
				// Flash just sets all fo the LEDs to the values input from the ADCs and flashes them
				cmdBuffer->redIntensity[LED_ptr] = redADC;				// Red
				cmdBuffer->grnIntensity[LED_ptr] = grnADC;				// Green
				cmdBuffer->bluIntensity[LED_ptr] = bluADC;				// Blue
				#endif
			}

			#ifdef FREERUN
		} else if (demoCounter < 25)
		{
			if (demoCounter == 20)
			{
				shotCounter = 1;
			}
			#else
		} else if (buttonMode == RANDOM)
		{
			#endif
			// Flashes all of the LEDs in the strip randomly
			cmdBuffer->subMode = RANDOM;
			cmdBuffer->period_mS = 100;
			cmdBuffer->modeParam = 512;		// This is the probability for how often to flash
			for (int LED_ptr=0;LED_ptr<NUM_LEDS;LED_ptr++)
			{
				#ifdef FREERUN
				cmdBuffer->redIntensity[LED_ptr] = 0xFF;				// Green
				cmdBuffer->grnIntensity[LED_ptr] = 0xFF;				// Red
				cmdBuffer->bluIntensity[LED_ptr] = 0xFF;				// Blue
				#else
				cmdBuffer->redIntensity[LED_ptr] = redADC;				// Red
				cmdBuffer->grnIntensity[LED_ptr] = grnADC;				// Green
				cmdBuffer->bluIntensity[LED_ptr] = bluADC;				// Blue
				#endif
			}
			#ifdef FREERUN
		} else if (demoCounter < 30)
		{
			if (demoCounter == 25)
			{
				shotCounter = 1;
			}
			#else
		} else if (buttonMode == THROB)
		{
			#endif
			cmdBuffer->subMode = THROB;
			cmdBuffer->period_mS = 62;
			cmdBuffer->modeParam = 4;			// The rate at which it grows and fades
			for (int LED_ptr=0;LED_ptr<NUM_LEDS;LED_ptr++)
			{
				#ifdef FREERUN
				cmdBuffer->redIntensity[LED_ptr] = 0xFF;				// Green
				cmdBuffer->grnIntensity[LED_ptr] = 0xFF;				// Red
				cmdBuffer->bluIntensity[LED_ptr] = 0xFF;				// Blue
				#else
				cmdBuffer->redIntensity[LED_ptr] = redADC;				// Red
				cmdBuffer->grnIntensity[LED_ptr] = grnADC;				// Green
				cmdBuffer->bluIntensity[LED_ptr] = bluADC;				// Blue
				#endif
			}

			#ifdef FREERUN
		} else if (demoCounter < 35)
		{
			if (demoCounter == 30)
			{
				shotCounter = 1;
			}
			#else
		} else if (buttonMode == FIRECRACKER)
		{
			#endif
			cmdBuffer->subMode = FIRECRACKER;
			cmdBuffer->period_mS = 62;
			for (int LED_ptr=0;LED_ptr<NUM_LEDS;LED_ptr++)
			{
				cmdBuffer->redIntensity[LED_ptr] = 0;				// Green
				cmdBuffer->grnIntensity[LED_ptr] = 0;				// Red
				cmdBuffer->bluIntensity[LED_ptr] = 0;				// Blue
			}
			#ifdef FREERUN
		} else if (demoCounter < 40)
		{
			if (demoCounter == 35)
			{
				shotCounter = 1;
			}
			#else
		} else if (buttonMode == ORBITALS)
		{
			shotCounter = 2;
			#endif
			cmdBuffer->subMode = ORBITALS;
			cmdBuffer->period_mS = 62;
			for (int LED_ptr=0;LED_ptr<NUM_LEDS;LED_ptr++)
			{
				cmdBuffer->redIntensity[LED_ptr] = 0;				// Green
				cmdBuffer->grnIntensity[LED_ptr] = 0;				// Red
				cmdBuffer->bluIntensity[LED_ptr] = 0;				// Blue
			}
		} else
		{
			demoCounter = 0;
		}
		if (shotCounter > 0)
		{
			appSendData();
			shotCounter--;
		}
		demoCounter++;
	}

	/*****************************************************************************
	Callback function from the network stack with received data
	*****************************************************************************/
	static bool appDataInd(NWK_DataInd_t *ind)
	{
		memcpy(appWorkingBuffer, ind, ind->size);
		appWorkingBufferLen = ind->size;
		appState = APP_STATE_RECD;

		return true;
	}
	#ifdef PHY_ENABLE_ENERGY_DETECTION
	/*****************************************************************************
	The controller will periodically scan the energy in each of the 2.4GHz channels
	and switch to the one with the lowest interference
	*****************************************************************************/
	static void channelScanTimerHandler(SYS_Timer_t *timer)
	{
		// This initializes for channel scan
		// Need to stop other functions while this runs
		SYS_TimerStop(&sendCmdTimer);
		SYS_TimerStop(&meshHeartbeatTimer);
		SYS_TimerStop(&pollInputsTimer);
		appState = APP_STATE_CHANNELSCAN;
		for (int LED_ptr=0;LED_ptr<NUM_LEDS*3;LED_ptr+=3)
		{
			LEDarray[LED_ptr] = 32;			// Green
			LEDarray[LED_ptr+1] = 0;		// Red
			LEDarray[LED_ptr+2] = 0;		// Blue
		}
		#ifdef DEBUG_PORT_E
		outPortE((int8_t) 0);
		outPortE((int8_t) 0xFF);
		#endif
		bestChannel = LOW_CHANNEL;
		currentChannel = LOW_CHANNEL;
		channelComplete = false;
		PHY_SetChannel(currentChannel);
		#ifdef DEBUG_IO
		HAL_GPIO_channelScanLED_set();
		#endif
		PHY_EdReq();
	}

	/*****************************************************************************
	Callback function from the network stack for energy detection measurement.
	Only one channel is processed at a time.  The global flag is used to tell
	the user that this channel is complete since the processing is asynchronous.
	The energy level is returned as a 16-bit signed value in dB.
	*****************************************************************************/
	void PHY_EdConf(int8_t energyLevel)
	{
		energyByte = energyLevel + 90;
		#ifdef DEBUG_IO
		HAL_GPIO_channelScanLED_clr();
		#endif
		#ifdef DEBUG_PORT_E
		outPortE((int8_t) 0);
		outPortE((int8_t) 0xFF);
		outPortE(currentChannel);
		outPortE((int8_t) 0);
		outPortE((int8_t) 0xFF);
		outPortE(currentChannel<<4);
		outPortE((int8_t) 0);
		outPortE((int8_t) 0xFF);
		outPortE(energyByte);
		outPortE((int8_t) 0);
		outPortE((int8_t) 0xFF);
		outPortE((energyByte<<4));
		#endif
		chanEnergy[currentChannel] = energyByte;
		LEDarray[(currentChannel-LOW_CHANNEL)*3] = 0;					// Green
		#ifdef DEBUG_PORT_E
		outPortE((int8_t) 0);
		outPortE((int8_t) 0xFF);
		outPortE(currentChannel-LOW_CHANNEL);
		outPortE((int8_t) 0);
		outPortE((int8_t) 0xFF);
		outPortE((currentChannel-LOW_CHANNEL)<<4);
		#endif
		LEDarray[(currentChannel-LOW_CHANNEL)*3+2] = energyByte;		// Convert from 2sC to absol
		if (energyByte < chanEnergy[bestChannel])
			bestChannel = currentChannel;
		channelComplete = true;
	}
	#endif
	/*****************************************************************************
	Initialization function, intended to be run only once as the node starts up
	*****************************************************************************/
	static void appInit(void)
	{
		// Either get the node address from EEPROM or from the config.h file
		#ifdef FIXED_ADDR
		myAddr = APP_ADDR;
		#else
		eeprom_busy_wait();
		ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
			myAddr = eeprom_read_word(&APP_ADDR);
		}
		#endif
		//
		// Initialize the network stack
		NWK_SetAddr(myAddr);
		NWK_SetPanId(APP_PANID);
		PHY_SetChannel(APP_CHANNEL);		// Start with default channel
		PHY_SetRxState(true);
		NWK_OpenEndpoint(LEDCmd_ENDPOINT, appDataInd);
		//
		// Define a timer that periodically triggers a command into the mesh
		sendCmdTimer.interval = APP_SEND_TIMER_INTERVAL;
		sendCmdTimer.mode = SYS_TIMER_PERIODIC_MODE;
		sendCmdTimer.handler = sendCmdTimerHandler;
		//
		// Define a timer that periodically triggers an empty command into the mesh
		meshHeartbeatTimer.interval = HEARTBEAT_INTERVAL;
		meshHeartbeatTimer.mode = SYS_TIMER_PERIODIC_MODE;
		meshHeartbeatTimer.handler = meshHeartbeatTimerHandler;
		//
		// Define a timer that periodically triggers a poll of the inputs
		pollInputsTimer.interval = IO_POLL_TIMER_INTERVAL;
		pollInputsTimer.mode = SYS_TIMER_PERIODIC_MODE;
		pollInputsTimer.handler = pollIOTimerHandler;
		//
		#ifdef PHY_ENABLE_ENERGY_DETECTION
		// Define a timer that periodically triggers a poll of the inputs
		channelScanTimer.interval = CHAN_SCAN_TIMER_INTERVAL;
		channelScanTimer.mode = SYS_TIMER_PERIODIC_MODE;
		channelScanTimer.handler = channelScanTimerHandler;
		SYS_TimerStart(&channelScanTimer);
		#endif
		//
		// Initialize the direction and state of all of the outputs
		// Output for LED serial data stream
		HAL_GPIO_lightStripData_out();
		HAL_GPIO_lightStripData_clr();
		#ifdef DEBUG_IO
		// These outputs drive LEDs or just digital outputs for visual status
		HAL_GPIO_channelScanLED_out();
		HAL_GPIO_channelScanLED_clr();
		#endif
		#ifdef BLINK_LEDS
		HAL_GPIO_hbLED_out();
		HAL_GPIO_hbLED_clr();
		HAL_GPIO_statusLED_out();
		HAL_GPIO_statusLED_clr();
		HAL_GPIO_sendStatusLED_out();
		HAL_GPIO_sendStatusLED_clr();
		#endif
		InitADC();
		HAL_GPIO_leftBttn_in();
		HAL_GPIO_leftBttn_pullup();
		HAL_GPIO_rightBttn_in();
		HAL_GPIO_rightBttn_pullup();
		HAL_GPIO_potPullup_out();
		HAL_GPIO_potPullup_set();
		currentLEDmode = LED_MODE_IDLE;
		redADC = 127;
		grnADC = 127;
		bluADC = 127;
		demoCounter = 0;

		// This is the initial value for the destination address for commands (for testing)
		targetAddr = BROADCAST_ADDR;
		// Initialize counter(s)
		mainLoopBlink = 0;
		// First thing to do is a channel scan
		#ifdef PHY_ENABLE_ENERGY_DETECTION
		appState = APP_STATE_CHANNELSCAN;
		for (int LED_ptr=0;LED_ptr<NUM_LEDS*3;LED_ptr+=3)
		{
			LEDarray[LED_ptr] = 32;			// Green
			LEDarray[LED_ptr+1] = 0;		// Red
			LEDarray[LED_ptr+2] = 0;		// Blue
		}
		bestChannel = LOW_CHANNEL;
		currentChannel = LOW_CHANNEL;
		channelComplete = false;
		PHY_SetChannel(currentChannel);
		PHY_EdReq();
		#else
		SYS_TimerStart(&sendCmdTimer);
		SYS_TimerStart(&meshHeartbeatTimer);
		SYS_TimerStart(&pollInputsTimer);
		#endif
	}	// end of appInit()
	/*****************************************************************************
	Task Handler - does whatever needs to be done on a regular basis as fast as possible
	*****************************************************************************/
	static void APP_TaskHandler(void)
	{
		//	appState implements a state machine
		//	As the app state changes (possibly via callback functions) there may be
		//	things that need to be done here.
		switch (appState)
		{
			case APP_STATE_INITIAL:
			{
				appInit();
				#ifdef PHY_ENABLE_ENERGY_DETECTION
				appState = APP_STATE_CHANNELSCAN;
				#else
				appState = APP_STATE_IDLE;
				#endif
			} break;

			#ifdef PHY_ENABLE_ENERGY_DETECTION
			// Find the best channel to use, by checking the energy level in each of the available
			// channels and choosing the lowest one.
			// Initialize the LED string to off
			case APP_STATE_CHANNELSCAN:
			{
				if (channelComplete && (currentChannel < HIGH_CHANNEL))
				{
					currentChannel++;
					channelComplete = false;
					PHY_SetChannel(currentChannel);
					#ifdef DEBUG_IO
					HAL_GPIO_channelScanLED_set();
					#endif
		#ifdef DEBUG_PORT_E
		outPortE((int8_t) 0);
		outPortE((int8_t) 0xFF);
		outPortE(currentChannel);
		outPortE((int8_t) 0);
		outPortE((int8_t) 0xFF);
		outPortE(currentChannel<<4);
		#endif
					PHY_EdReq();
				}
				else if (channelComplete)
				{
					#ifdef DEBUG_IO
					HAL_GPIO_channelScanLED_set();
					#endif
		#ifdef DEBUG_PORT_E
		outPortE((int8_t) 0);
		outPortE((int8_t) 0xFF);
		outPortE(currentChannel);
		outPortE((int8_t) 0);
		outPortE((int8_t) 0xFF);
		outPortE(currentChannel<<4);
		outPortE((int8_t) 0);
		outPortE((int8_t) 0xFF);
		outPortE(bestChannel);
		outPortE((int8_t) 0);
		outPortE((int8_t) 0xFF);
		outPortE(bestChannel<<4);
		outPortE((int8_t) 0);
		outPortE((int8_t) 0xFF);
		#endif
					PHY_SetChannel(bestChannel);
					LEDarray[(bestChannel-LOW_CHANNEL)*3+1] = 32;		// Show selected channel in red
					updateLEDs(LEDarray, NUM_LEDS*3);
					appState = APP_STATE_IDLE;
					SYS_TimerStart(&sendCmdTimer);
					SYS_TimerStart(&meshHeartbeatTimer);
					SYS_TimerStart(&pollInputsTimer);
					#ifdef DEBUG_IO
					HAL_GPIO_channelScanLED_clr();
					#endif
				}
			}
			break;
			#endif

			case APP_STATE_IDLE:
			break;

			default:
			break;
		}
	}

	/*****************************************************************************
	Main Program - main loop that executes non-time-critical things
	*****************************************************************************/
	int main(void)
	{
		// Initialize the system and network function infrastructure
		SYS_Init();
		appState = APP_STATE_INITIAL;
		// Then loops indefinitely, alternating between processing system tasks and app tasks
		while (1)
		{
			// Execute all of the pending system and network stacks that make everything else work
			// Note that this is where callbacks will be executed!
			SYS_TaskHandler();
			// Go do whatever the app state machine says is needed, if anything
			// There may be nothing to do here if periodic timer handlers and callbacks can
			// accomplish everything in an as-needed manner without polling.
			APP_TaskHandler();
			// This is just the heartbeat LED output that visually tells the observer that
			// the application loop is running and also gives an indication of the loop execution
			// time.  Each time the LED changes state, 25,000 loops have been executed.  For each on-off
			// combination, that's 50,000 loops.
			mainLoopBlink++;
			if (mainLoopBlink > 25000) {
				mainLoopBlink = 0;
				#ifdef BLINK_LEDS
				HAL_GPIO_hbLED_toggle();
				#endif
			}
		}
	}
