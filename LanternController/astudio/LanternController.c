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
	APP_STATE_RECD
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

static void appSendData(void);
extern void InitADC (void);
extern uint8_t GetADC (uint8_t channel);

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

static uint8_t chanEnergy[NUM_CHANNELS];
static uint8_t currentChannel;
static bool channelComplete;
static uint8_t bestChannel;

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
	if ((req->status == NWK_SUCCESS_STATUS)||(req->status == NWK_NO_ACK_STATUS)||(req->status == NWK_PHY_NO_ACK_STATUS)) {
	  	HAL_GPIO_sendStatusLED_set();
	} else
	  	HAL_GPIO_sendStatusLED_clr();
		  
	appDataReqBusy = false;
}

/*****************************************************************************
// The function used to send data to other nodes in the mesh.  Currently only
// sends the node address out on broadcast.  If some other message is to be sent,
// then this will either need to be modified, or a new function will be needed.
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
		appDataReq.options = NWK_OPT_ENABLE_SECURITY;
	} else
	{
		appDataReq.options = NWK_OPT_ACK_REQUEST | NWK_OPT_ENABLE_SECURITY;
	}
	appDataReq.data = appDataReqBuffer;
	appDataReq.size = sizeof(LED_Command_t);
	appDataReq.confirm = appDataConf;
	NWK_DataReq(&appDataReq);

	HAL_GPIO_statusLED_toggle();
  	HAL_GPIO_sendStatusLED_clr();

	appWorkingBufferPtr = 0;
	appDataReqBusy = true;

	targetAddr = BROADCAST_ADDR;
}

/*****************************************************************************
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
// This is a callback function that is triggered when the periodic timer says
// it's time to send out the next command to the mesh.
*****************************************************************************/

static void sendCmdTimerHandler(SYS_Timer_t *timer)
{
	cmdBuffer = appWorkingBuffer;
	cmdBuffer->mode = MODE_GLOBAL;
#ifdef FREERUN
	if (demoCounter < 1)
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
		cmdBuffer->period_mS = 255;
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
	} else if (demoCounter < 20)
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
		cmdBuffer->period_mS = 255;
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
	} else if (demoCounter < 15)
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
			if (redADC > (LED_ptr*16))
			{
				cmdBuffer->redIntensity[LED_ptr] = redADC-(LED_ptr*16);				// Red
			} else {
				cmdBuffer->redIntensity[LED_ptr] = 0;
			}
			if (grnADC > (LED_ptr*16))
			{
				cmdBuffer->grnIntensity[LED_ptr] = grnADC-(LED_ptr*16);				// Red
			} else {
				cmdBuffer->grnIntensity[LED_ptr] = 0;
			}
			if (bluADC > (LED_ptr*16))
			{
				cmdBuffer->bluIntensity[LED_ptr] = bluADC-(LED_ptr*16);				// Red
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
		cmdBuffer->modeParam = 4;			// The rate at which is throbs
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
//			if (ind->dstEndpoint = ledCommand) {
	//				if  (ind->data[1] = ledBrightness) {
	memcpy(appWorkingBuffer, ind, ind->size);
	appWorkingBufferLen = ind->size;
	appState = APP_STATE_RECD;

	return true;
}

/*****************************************************************************
	Callback function from the network stack for energy detection measurement
*****************************************************************************/
void PHY_EdConf(int8_t energyLevel)
{
	chanEnergy[currentChannel] = energyLevel;
	channelComplete = true;
}

/*****************************************************************************
	Initialization function, intended to be run only once as the node starts up
*****************************************************************************/
static void appInit(void)
{
// Either get the node address from EEPROM or from the config.h file
#ifndef FIXED_ADDR
	eeprom_busy_wait();
	ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
		myAddr = eeprom_read_word(&APP_ADDR);
	}
#else
	myAddr = APP_ADDR;
#endif
//
// Initialize the network stack
	NWK_SetAddr(myAddr);
	NWK_SetPanId(APP_PANID);
	for (currentChannel=LOW_CHANNEL;currentChannel<=HIGH_CHANNEL;currentChannel++)
	{
		PHY_SetChannel(currentChannel);
		channelComplete = false;
		PHY_EdReq();
		while (!channelComplete)
		    SYS_TaskHandler();
	}
	PHY_SetChannel(APP_CHANNEL);
	PHY_SetRxState(true);
	NWK_OpenEndpoint(APP_ENDPOINT, appDataInd);
//
// Define a timer that periodically triggers a command into the mesh
	sendCmdTimer.interval = APP_SEND_TIMER_INTERVAL;
	sendCmdTimer.mode = SYS_TIMER_PERIODIC_MODE;
	sendCmdTimer.handler = sendCmdTimerHandler;
	SYS_TimerStart(&sendCmdTimer);
//
// Define a timer that periodically triggers a command into the mesh
	meshHeartbeatTimer.interval = COMMAND_TIMEOUT_INTERVAL;
	meshHeartbeatTimer.mode = SYS_TIMER_PERIODIC_MODE;
	meshHeartbeatTimer.handler = meshHeartbeatTimerHandler;
	SYS_TimerStart(&meshHeartbeatTimer);
//
// Define a timer that periodically triggers a poll of the inputs
	pollInputsTimer.interval = IO_POLL_TIMER_INTERVAL;
	pollInputsTimer.mode = SYS_TIMER_PERIODIC_MODE;
	pollInputsTimer.handler = pollIOTimerHandler;
	SYS_TimerStart(&pollInputsTimer);
//
// These outputs drive LEDs or just digital outputs for visual status
	HAL_GPIO_hbLED_out();
	HAL_GPIO_hbLED_clr();
	HAL_GPIO_statusLED_out();
	HAL_GPIO_statusLED_clr();
	HAL_GPIO_sendStatusLED_out();
	HAL_GPIO_sendStatusLED_clr();
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
// Find the best channel to use
/*
	bestChannel = LOW_CHANNEL;
	for (currentChannel=LOW_CHANNEL;currentChannel<=HIGH_CHANNEL;currentChannel++)
	{
		PHY_SetChannel(currentChannel);
		channelComplete = false;
		PHY_EdReq();
		while (!channelComplete)
		SYS_TaskHandler();
		if (chanEnergy[bestChannel] > chanEnergy[currentChannel])
		bestChannel = currentChannel;
	}
	PHY_SetChannel(bestChannel);
*/
}
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
			appState = APP_STATE_IDLE;
		} break;

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
		HAL_GPIO_hbLED_toggle();
	}
  }
}
