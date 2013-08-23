/*
 * \file FoolsLantern.c
 *
 * \brief App framework for FestiFools lantern codes
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
	APP_STATE_DATARDY,
	APP_STATE_LOCAL,
	NWK_STATE_INITIAL,
	NWK_STATE_IDLE,
	NWK_STATE_WAITSEND,
	NWK_STATE_RECD
} AppState_t;

/*****************************************************************************
		Function prototypes
*****************************************************************************/

extern void updateLEDs (uint8_t colorArray[], uint16_t numLEDs);
extern void outPortE (uint8_t diagInfo);

static void appSendAddr(void);
static void appAddrCheckSendData(void);


/*****************************************************************************
		Variables
*****************************************************************************/

static uint16_t myAddr;

#ifndef FIXED_ADDR
static uint16_t APP_ADDR EEMEM;
#endif

static AppState_t appState = APP_STATE_INITIAL;
static AppState_t nwkState = NWK_STATE_INITIAL;
static SYS_Timer_t addrCheckTimer;
static SYS_Timer_t animationTimer;
static SYS_Timer_t accelerationTimer;
static SYS_Timer_t cmdTimer;
static SYS_Timer_t channelTimer;
static NWK_DataReq_t appDataReq;
static NWK_DataReq_t appSyncReq;
static bool appDataReqBusy = false;
static bool appSyncReqBusy = false;
static bool cmdTimeout;
static uint8_t appDataReqBuffer[APP_BUFFER_SIZE];
static uint8_t appWorkingBuffer[APP_BUFFER_SIZE];
static uint8_t appWorkingBufferPtr = 0;
static uint16_t targetAddr;

static uint8_t currentChannel;

static LED_Command_t *cmdBuffer;
static int	cmdBufferPtr;
static uint8_t tempRed;
static uint8_t tempGrn;
static uint8_t tempBlu;
static uint8_t flashState;
static uint16_t randomPtr;
static uint16_t randomFreq;
static uint8_t throbDelta;
static uint8_t throbSum;
static uint8_t throbFade;
static uint8_t throbTimerAccel;
static uint8_t fuse;
static uint8_t fuseChange;
static uint16_t animationTimerPeriod;
static bool pulseOn;
static bool syncOn;

static uint8_t LEDarray[NUM_LEDS*3];
static uint8_t LEDpattern[NUM_LEDS*3];
static uint8_t currentLEDmode;
static uint8_t latestRSSI;
static uint8_t averageRSSI;			//to save previous value, this might need to be global

static uint16_t mainLoopBlink;
static uint16_t debug1Blink;
static uint16_t debug2Blink;
static uint16_t debug3Blink;
static uint16_t debug4Blink;
static uint8_t blanking;
static uint8_t debugValue;
static uint8_t debugStart;

/*****************************************************************************
		Function implementations
*****************************************************************************/
/*****************************************************************************
*****************************************************************************/
static void appAddrConf(NWK_DataReq_t *req)
{
	appDataReqBusy = false;
}

/*****************************************************************************
*****************************************************************************/
static void appSendAddr(void)
{
	if (appDataReqBusy || 0 == appWorkingBufferPtr)
		return;

	memcpy(appDataReqBuffer, appWorkingBuffer, appWorkingBufferPtr);

	appDataReq.dstAddr = BROADCAST_ADDR;
	appDataReq.dstEndpoint = Mote_Addr_ENDPOINT;
	appDataReq.srcEndpoint = Mote_Addr_ENDPOINT;
	appDataReq.options = 0;
	appDataReq.data = appWorkingBuffer;
	appDataReq.size = appWorkingBufferPtr;
	appDataReq.confirm = appAddrConf;
	NWK_DataReq(&appDataReq);

	appWorkingBufferPtr = 0;
	appDataReqBusy = true;
}

/*****************************************************************************
*****************************************************************************/
static void appSyncConf(NWK_DataReq_t *req)
{
	appSyncReqBusy = false;
}

/*****************************************************************************
*****************************************************************************/
static void appSendSync(void)
{
	if (appSyncReqBusy)
		return;

//	memcpy(appDataReqBuffer, appWorkingBuffer, appWorkingBufferPtr);
	appWorkingBufferPtr = 1;
	appSyncReq.dstAddr = BROADCAST_ADDR;
	appSyncReq.dstEndpoint = SyncCmd_ENDPOINT;
	appSyncReq.srcEndpoint = SyncCmd_ENDPOINT;
	appSyncReq.options = 0;
	appSyncReq.data = appWorkingBuffer;
	appSyncReq.size = appWorkingBufferPtr;
	appSyncReq.confirm = appSyncConf;
	NWK_DataReq(&appSyncReq);

	appWorkingBufferPtr = 0;
	appSyncReqBusy = true;
	
// Clear the timeout flag so we don't switch to local mode
	cmdTimeout = false;
// Turn on the sync command flag
	syncOn = true;
// Set the mode to locked to command node
	appState = APP_STATE_LOCAL;
// Sync mode is preset local accelerating throb
	currentLEDmode = THROB;
	throbDelta = 2;
	throbFade = 1;
	for (int LED_ptr=0;LED_ptr<NUM_LEDS*3;LED_ptr+=3)
	{
		LEDarray[LED_ptr] = 0;				// Green
		LEDpattern[LED_ptr] = 0;			// Green
		LEDarray[LED_ptr+1] = 0;			// Red
		LEDpattern[LED_ptr+1] = 0;			// Red
		LEDarray[LED_ptr+2] = 196;			// Blue
		LEDpattern[LED_ptr+2] = 196;		// Blue
	}
	animationTimer.interval = 125;
	throbTimerAccel = 1;

}

/*****************************************************************************
*****************************************************************************/
static void addrCheckTimerHandler(SYS_Timer_t *timer)
{
}

/*****************************************************************************
*****************************************************************************/
static void accelerationTimerHandler(SYS_Timer_t *timer)
{
	if (currentLEDmode == THROB)
	{
		if (throbTimerAccel != 0)		// Update the throb period if accelerating
		{
			if (animationTimerPeriod > 50)
			{
				animationTimerPeriod -= throbTimerAccel;
				if (animationTimerPeriod < 50)
				animationTimerPeriod = 50;
				animationTimer.interval = animationTimerPeriod;
			} else if (throbDelta < 5)
			{
				throbDelta++;
			}
		}
	}
}

/*****************************************************************************
	Callback function from the timer subsystem.  The timer is set to periodically
	invoke this function when scanning for a channel with a controller on it.
*****************************************************************************/
static void channelTimerHandler(SYS_Timer_t *timer)
{
//	If this timer goes off, it means that no commands were received on the current
//	channel (in LOCAL mode), and the node should switch to the next channel
	currentChannel++;
	if (currentChannel > HIGH_CHANNEL)
		currentChannel = LOW_CHANNEL;
	PHY_SetChannel(currentChannel);
}

/*****************************************************************************
	Callback function from the timer subsystem.  The timer is set to periodically
	invoke this function to update something.
*****************************************************************************/
static void cmdTimerHandler(SYS_Timer_t *timer)
{
// If the cmdTimeout flag is set when this timer goes off, it means that no commands
// have been received since the last time.  In that case, this node switches into the
// default local mode.
	if (cmdTimeout)
	{
		appState = APP_STATE_LOCAL;
		SYS_TimerStart(&channelTimer);
//		Put the LEDs to sleep to save power
		currentLEDmode = THROB;
		throbDelta = 2;
		throbFade = 1;
		for (int LED_ptr=0;LED_ptr<NUM_LEDS*3;LED_ptr+=3)
		{
			LEDarray[LED_ptr] = 0;				// Green
			LEDpattern[LED_ptr] = 0;			// Green
			LEDarray[LED_ptr+1] = 0;			// Red
			LEDpattern[LED_ptr+1] = 0;			// Red
			LEDarray[LED_ptr+2] = 196;			// Blue
			LEDpattern[LED_ptr+2] = 196;		// Blue
		}
		animationTimer.interval = 125;
		throbTimerAccel = 1;
// Otherwise, the flag was reset by a command that was received.  In this case, the flag
// is set again to see if a command is received in the next timer interval.  So the node
// is periodically checking to see that some command has been received from outside in
// the mesh.  As long as something is going on, it will not drop into the local default mode.
	} else
	{
		cmdTimeout = true;
		SYS_TimerStop(&channelTimer);
	}
}

/*****************************************************************************
	Callback function from the timer subsystem.  The timer is set to periodically
	invoke this function to update the LED pattern according to the current mode.
*****************************************************************************/
static void appLEDAnimationTimerHandler(SYS_Timer_t *timer)
{
	switch(currentLEDmode)
	{
		case STATIC:
		{
//			If the pattern is static (no animation) then there's nothing to do			
		} break;
		case FLASH:
		{
//			This animation simply alternates between the provided pattern and all LEDs off
//			The flashState variable keeps track of whether LEDs should be on or off on this cycle
			if (flashState == 0)
			{
				flashState = 1;
				for (int LED_ptr=0;LED_ptr<NUM_LEDS*3;LED_ptr+=3)
				{
					LEDarray[LED_ptr] = 0;
					LEDarray[LED_ptr+1] = 0;
					LEDarray[LED_ptr+2] = 0;
				}
			} else
			{
				flashState = 0;
				memcpy(LEDarray,LEDpattern,NUM_LEDS*3);
			}

			updateLEDs(LEDarray, NUM_LEDS*3);
		} break;
		case ROTATE:
		{
			tempGrn = LEDarray[0];
			tempRed = LEDarray[1];
			tempBlu = LEDarray[2];
			for (int LED_ptr=0;LED_ptr<NUM_LEDS*3;LED_ptr+=3)
			{
				LEDarray[LED_ptr] = LEDarray[LED_ptr+3];
				LEDarray[LED_ptr+1] = LEDarray[LED_ptr+4];
				LEDarray[LED_ptr+2] = LEDarray[LED_ptr+5];
			}
			LEDarray[(NUM_LEDS*3)-3] = tempGrn;
			LEDarray[(NUM_LEDS*3)-2] = tempRed;
			LEDarray[(NUM_LEDS*3)-1] = tempBlu;

			updateLEDs(LEDarray, NUM_LEDS*3);
		} break;
		case THROB:
		{
//			This animation will make the pattern grow and fade in intensity to make a throbbing
//			effect.  The rate is set by the throbDelta parameter.  Current implementation just
//			keeps subtracting the delta value from the color intensities and lets the values wrap
//			around.  This has the effect of changing the color as well as the overall intensity.
			throbSum = 0;
//			The throbState variable indicates whether this is the build or fade direction.  
//			throbState = 0 means that the direction is build, so the intensity of each color is
//			reduced by the throbDelta amount until it is zero.
			if (throbFade > 0)
			{
				for (int LED_ptr=0;LED_ptr<NUM_LEDS*3;LED_ptr++)
				{
					if (LEDarray[LED_ptr] >= throbDelta)
					{
						LEDarray[LED_ptr] -= throbDelta;
						throbSum++;
					} else {
						LEDarray[LED_ptr] = 0;
					}
				}
				if (throbSum == 0)		// Check if any changes were made this pass
				{
					throbFade = 0;		// When all LEDs are off, switch direction to build
				}
			} else {
				for (int LED_ptr=0;LED_ptr<NUM_LEDS*3;LED_ptr++)
				{
					if ((LEDpattern[LED_ptr] - LEDarray[LED_ptr]) > throbDelta)
					{
						LEDarray[LED_ptr] += throbDelta;
						throbSum++;		// Increment the counter for each LED color that is changed
					} else {
						LEDarray[LED_ptr] = LEDpattern[LED_ptr];
					}
				}
				if (throbSum == 0)		// Check if any changes were made this pass
				{
					throbFade = 1;		// When all LEDs are at target brightness, switch direction to fade
				}
			}
			updateLEDs(LEDarray, NUM_LEDS*3);
		} break;
		case RANDOM:
		{
//			This animation flashes the pattern at a rate determined by the randomFreq parameter
//			Since the range of values from the random number generator is 0 - 32767, the frequency
//			of flashes is roughly randomFreq/32768*1000/period_mS.
			randomPtr = rand();
			if (randomPtr <= randomFreq)
			{
				memcpy(LEDarray,LEDpattern,NUM_LEDS*3);
			} else
			{
				for (int LED_ptr=0;LED_ptr<NUM_LEDS*3;LED_ptr+=3)
				{
					LEDarray[LED_ptr] = 0;
					LEDarray[LED_ptr+1] = 0;
					LEDarray[LED_ptr+2] = 0;
				}
			}			

			updateLEDs(LEDarray, NUM_LEDS*3);
		} break;
		case FIRECRACKER:
		{
//			fuse hopefully regulates how long the pattern lasts
			fuse = 100;
			fuseChange = 0;

			while(fuse > 0)
			{
//				start at yellow
				fuseChange = 0;
				for(int LED_ptr=0; LED_ptr<NUM_LEDS*3; LED_ptr+=3)
				{
					LEDarray[LED_ptr] = 170 + fuseChange;		// Grn
					LEDarray[LED_ptr+1] = 175 + fuseChange;		// Red
					LEDarray[LED_ptr+2] = 0;					// Blu
				}
//				incr change by 5, going toward white
				fuseChange+=5;
//				use bounded rand to insert random blips of red sparks
//				insert random pops! (with a few guaranteed pops)
				if(fuse == (rand() % fuse) || fuse % 13 == 0)
				{
//					every 3rd LED
					for(int LED_ptr=0; LED_ptr<NUM_LEDS*3; LED_ptr+=3)
					{
						LEDarray[LED_ptr] = 0;		// Grn
						LEDarray[LED_ptr+1] = 255;	// red
						LEDarray[LED_ptr+2] = 0;	//blu
					}
				}
				fuse--;
			}

			updateLEDs(LEDarray, NUM_LEDS*3);
		} break;
		case ORBITALS:
		{
//			(looks like throb color change, but change is tied to RSSI value change rather than hardcoded delta)
//			check to see if signal is getting closer or farther
			for (int LED_ptr=0;LED_ptr<NUM_LEDS*3;LED_ptr+=3)
			{
				LEDarray[LED_ptr] = 0;
				LEDarray[LED_ptr+1] = averageRSSI;
				LEDarray[LED_ptr+2] = 255-averageRSSI;
			}

/*
			if(latestRSSI > prevRSSI) //getting closer
			{
				for(int LED_ptr=0; LED_ptr<NUM_LEDS*3; LED_ptr +=3)
				{
					LEDarray[LED_ptr] += latestRSSI;
				}
			} else
			{
				for(int LED_ptr=0; LED_ptr<NUM_LEDS*3; LED_ptr +=3)
				{
					LEDarray[LED_ptr] -= latestRSSI;
				}
			}
*/
			updateLEDs(LEDarray, NUM_LEDS*3);
		} break;
// This is a simple pulse mode
		case ONESHOT:
		{
			// Pulses the LEDs and then goes dark
			if(pulseOn) //getting closer
			{
				pulseOn = false;
				for(int LED_ptr=0; LED_ptr<NUM_LEDS*3; LED_ptr++)
				{
					LEDarray[LED_ptr] = 0;
				}
				updateLEDs(LEDarray, NUM_LEDS*3);
			}
		} break;
//		This default case should never be executed if all of the modes have been implemented!
		default:
			break;
	}
}

/*****************************************************************************
	Callback function from the network stack for the LED Command app endpoint
	Messages that carry commands go through this function
*****************************************************************************/
static bool LEDCmdDataInd(NWK_DataInd_t *ind)
{
// Clear the timout flag so we don't switch to local mode
	cmdTimeout = false;
// Make sure the pointer is set correctly
	cmdBuffer = &appWorkingBuffer[0];
// Copy the data from the message buffer into the command buffer so that the
// network buffer can be freed up and re-used
	memcpy(appWorkingBuffer, ind->data, ind->size);
//	debugStart = ind->size;
	nwkState = NWK_STATE_RECD;
	appState = APP_STATE_DATARDY;
// This is the received signal strength value which might be useful	for some
// swarm rule(s)
	latestRSSI = (ind->rssi + 90) * 9;
	averageRSSI = (averageRSSI>>2)*3 + latestRSSI;
// Returning "true" to the network stack says that this message should be acknowledged
// if the sender requested it.
	return true;
}

/*****************************************************************************
	Callback function from the network stack for the start / sync command
*****************************************************************************/
static bool SyncDataInd(NWK_DataInd_t *ind)
{
	HAL_GPIO_statusLED_toggle();
// Turn on the sync command flag
	syncOn = true;
// Set the mode to locked to command node
	appState = APP_STATE_LOCAL;
// Sync mode is preset to local accelerating throb
	currentLEDmode = THROB;
	throbDelta = 2;
	throbFade = 1;
	for (int LED_ptr=0;LED_ptr<NUM_LEDS*3;LED_ptr+=3)
	{
		LEDarray[LED_ptr] = 0;				// Green
		LEDpattern[LED_ptr] = 0;			// Green
		LEDarray[LED_ptr+1] = 0;			// Red
		LEDpattern[LED_ptr+1] = 0;			// Red
		LEDarray[LED_ptr+2] = 196;			// Blue
		LEDpattern[LED_ptr+2] = 196;		// Blue
	}
	animationTimer.interval = 125;
	throbTimerAccel = 1;

// Returning "true" to the network stack says that this message should be acknowledged
// if the sender requested it.
	return true;
}

/*****************************************************************************
	Callback function from the network stack for addr check response
*****************************************************************************/

static bool addrCheckDataConf(NWK_DataReq_t *req)
{
// This is the response to the address inquiry to see if there is another node out there using
//	the same address.  If there is, i.e., there is a successful acknowledgement, then this node
//	changes its address.
	if (req->status == NWK_SUCCESS_STATUS)
	{
		#ifndef FIXED_ADDR
		eeprom_busy_wait();
		ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
			myAddr = eeprom_read_word(&APP_ADDR);
			if ((myAddr == 0) || (myAddr == 0xFFFF))
			{
				//			This would only be executed if the EEPROM location is 0 or not programmed
				myAddr = rand();
			} else
			{
				//			If there is another node using this address on the mesh, just increment the local address.
				//			 Eventually this will be checked again for a collision.
				myAddr++;
			}
			// Save the new address in EEPROM
			eeprom_write_word(&APP_ADDR, myAddr);
		}
		// Update the network stack with the new address
		NWK_SetAddr(myAddr);
		#endif
	} else if ((req->status == NWK_NO_ACK_STATUS)||(req->status == NWK_PHY_NO_ACK_STATUS))
	{
	} else
	{
	}
	// Clear the pending request flag
	appDataReqBusy = false;
}

/*****************************************************************************
// The function used to send a duplicate address check
*****************************************************************************/
static void appAddrCheckSendData(void)
{
	if (appDataReqBusy)
	return;

//TODO
// Need some kind of key here to distinguish msgs that might get sent to self!
	memcpy(appDataReqBuffer, appWorkingBuffer, sizeof(LED_Command_t));

	appDataReq.dstAddr = 0xFFFF;
	appDataReq.dstEndpoint = AddrCheck_ENDPOINT;
	appDataReq.srcEndpoint = AddrCheck_ENDPOINT;
	if (targetAddr == BROADCAST_ADDR)
	{
		appDataReq.options = NWK_OPT_ENABLE_SECURITY;
	} else
	{
		appDataReq.options = NWK_OPT_ACK_REQUEST | NWK_OPT_ENABLE_SECURITY;
	}
	appDataReq.data = appDataReqBuffer;
	appDataReq.size = sizeof(LED_Command_t);
	appDataReq.confirm = addrCheckDataConf;
	NWK_DataReq(&appDataReq);

	HAL_GPIO_statusLED_toggle();
	HAL_GPIO_sendStatusLED_clr();

	appWorkingBufferPtr = 0;
	appDataReqBusy = true;

	targetAddr = BROADCAST_ADDR;
}

/*****************************************************************************
	Initialize all of the application-related states
*****************************************************************************/
static void appInit(void)
{
// This allows the local node address to come from either EEPROM or the config.h file
#ifndef FIXED_ADDR
	eeprom_busy_wait();
	ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {	
		myAddr = eeprom_read_word(&APP_ADDR);
		if ((myAddr == 0) || (myAddr == 0xFFFF))
		{
			myAddr = rand();
		}
		eeprom_write_word(&APP_ADDR, myAddr);
	}
#else
	myAddr = APP_ADDR;
#endif
// Set the seed for the random number generator using the local address
	srand(myAddr);
// Set up the system and network for the application
	NWK_SetAddr(myAddr);
	NWK_SetPanId(APP_PANID);
	currentChannel = APP_CHANNEL;
	PHY_SetChannel(currentChannel);
	PHY_SetRxState(true);
	NWK_OpenEndpoint(LEDCmd_ENDPOINT, LEDCmdDataInd);
	NWK_OpenEndpoint(SyncCmd_ENDPOINT, SyncDataInd);
// Implement a periodic timer to check for duplicate addresses in the mesh
	addrCheckTimer.interval = ADDR_CHECK_INTERVAL;
	addrCheckTimer.mode = SYS_TIMER_PERIODIC_MODE;
	addrCheckTimer.handler = addrCheckTimerHandler;
	SYS_TimerStart(&addrCheckTimer);
// Implement the timer to determine when to switch to local mode if
// no commands are received.
	cmdTimer.interval = COMMAND_TIMEOUT_INTERVAL;
	cmdTimer.mode = SYS_TIMER_PERIODIC_MODE;
	cmdTimer.handler = cmdTimerHandler;
	SYS_TimerStart(&cmdTimer);
// Implement the timer that determines how often the LEDs are changed
// if there is a pattern that flashes, rotates, etc.  Note that the
// interval is only initialized here.  It can be changed on-the-fly by
// the application.  For example, the LED command contains an update
// interval for exactly this purpose.
	animationTimer.interval = LED_ANIMATION_INTERVAL;
	animationTimer.mode = SYS_TIMER_PERIODIC_MODE;
	animationTimer.handler = appLEDAnimationTimerHandler;
	SYS_TimerStart(&animationTimer);
// Implement the timer to determine when to switch to local mode if
// no commands are received.
	accelerationTimer.interval = ACCELERATION_INTERVAL;
	accelerationTimer.mode = SYS_TIMER_PERIODIC_MODE;
	accelerationTimer.handler = accelerationTimerHandler;
	SYS_TimerStart(&cmdTimer);
// Implement the timer to determine the time between channels when scanning
// for a controller.
	channelTimer.interval = CHANNEL_SCAN_INTERVAL;
	channelTimer.mode = SYS_TIMER_PERIODIC_MODE;
	channelTimer.handler = channelTimerHandler;
	SYS_TimerStart(&channelTimer);
// These are visual outputs that will be used to see what's going on
	HAL_GPIO_hbLED_out();
	HAL_GPIO_hbLED_clr();
	HAL_GPIO_statusLED_out();
	HAL_GPIO_statusLED_clr();
	HAL_GPIO_rcvLED_out();
	HAL_GPIO_rcvLED_clr();
	HAL_GPIO_lightStripData_out();
	HAL_GPIO_lightStripData_clr();
	HAL_GPIO_debug1_out();
	HAL_GPIO_debug1_clr();
	HAL_GPIO_debug2_out();
	HAL_GPIO_debug2_clr();
	HAL_GPIO_debug3_out();
	HAL_GPIO_debug3_clr();
	HAL_GPIO_debug4_out();
	HAL_GPIO_debug4_clr();
	HAL_GPIO_syncInput_in();
	HAL_GPIO_syncInput_pullup();
// Counters that make things that happen frequently more visible to
// an observer. This will make the outputs blink, rather than just
// appear dimmer or brighter.
	debug1Blink = 0;
	debug2Blink = 0;
	debug3Blink = 0;
	debug4Blink = 0;
// Initialize the animation variables / parameters.  That way if a parameter is not provided
// via the command message, there is a default value for it.
//		Default to static / fixed mode to start
	currentLEDmode = STATIC;
//		For the random mode(s)
	randomFreq = 512;
//		For the throb mode
	throbDelta = 4;
// Initialize the LED string to 1/4 brightness, white color
	for (int LED_ptr=0;LED_ptr<NUM_LEDS*3;LED_ptr+=3)
	{
		LEDarray[LED_ptr] = 0;			// Green
		LEDarray[LED_ptr+1] = 0;		// Red
		LEDarray[LED_ptr+2] = 0;		// Blue
	}
	updateLEDs(LEDarray, NUM_LEDS*3);
	syncOn = false;
// Initialize the state of the flag that says the network is available for transmission
	appDataReqBusy = false;
	
// Broadcast this mote's address
	appWorkingBufferPtr = 2;
	appWorkingBuffer[0] = APP_ADDR;
	appWorkingBuffer[1] = 0;
	appSendAddr();
//. Initialize the buffer length
	appWorkingBufferPtr = 0;
// This is a flag used for flashing alternate status outputs on the debug port
//	blanking = 1;
	}

/*****************************************************************************
		Task Handler
*****************************************************************************/
static void APP_TaskHandler(void)
{
// The app is implemented via a state machine which depends upon the appState
// variable to hold the current value
    switch (appState)
	{
// This should ensure that the initialization function is only execute once
		case APP_STATE_INITIAL:
		{
			appInit();
			appState = APP_STATE_IDLE;
		} break;
// This is the do-nothing case
		case APP_STATE_IDLE:
			if (!HAL_GPIO_syncInput_read())
			{
				appSendSync();
			}
		break;
// This is the default local mode for when there is no command node
		case APP_STATE_LOCAL:
		{
			if (!HAL_GPIO_syncInput_read())
			{
				HAL_GPIO_statusLED_toggle();
				appSendSync();
			}
		} break;
// When a new command is received, this state is set by the callback function handling
// messages on the LED command app endpoint
		case APP_STATE_DATARDY:
		{
			if (cmdBuffer->mode == MODE_GLOBAL)
			{
//				Set up the common parameters provided by the command message
				currentLEDmode = cmdBuffer->subMode;
				switch (currentLEDmode)
				{
					case RANDOM:
					{
						randomFreq = cmdBuffer->modeParam;
					}	break;
					case THROB:
					{
						throbDelta = cmdBuffer->modeParam;
						throbFade = 1;
						throbTimerAccel = 0;
					}	break;
					case ONESHOT:
					{
						pulseOn = true;
					}	break;
					default:
					break;
				}
				animationTimer.interval = cmdBuffer->period_mS;
				animationTimerPeriod = cmdBuffer->period_mS;

//				This mode is fixed color mode where command provides a color pattern
				cmdBufferPtr = 0;
				for (int LED_ptr=0;LED_ptr<NUM_LEDS*3;LED_ptr+=3)
				{
					LEDarray[LED_ptr] = cmdBuffer->grnIntensity[cmdBufferPtr];			// Green
					LEDpattern[LED_ptr] = LEDarray[LED_ptr];
					LEDarray[LED_ptr+1] = cmdBuffer->redIntensity[cmdBufferPtr];		// Red
					LEDpattern[LED_ptr+1] = LEDarray[LED_ptr+1];
					LEDarray[LED_ptr+2] = cmdBuffer->bluIntensity[cmdBufferPtr];		// Blue
					LEDpattern[LED_ptr+2] = LEDarray[LED_ptr+2];
					cmdBufferPtr++;
				}
				updateLEDs(LEDarray, NUM_LEDS*3);
				appState = APP_STATE_IDLE;

// This is for the non-centrally controlled operation.  It should be the default when the node
// starts up and does not get any communications from a controller.
			} else if (cmdBuffer->mode == MODE_PEER_TO_PEER)
			{
//					updateLEDs(LEDarray, NUM_LEDS);
// This is used to clear the command timeout
			} else if (cmdBuffer->mode == MODE_NOCHANGE)
			{
				cmdTimeout = false;
// This should never be executed, because the state should always be defined as one of the
// previous cases!
			} else
			{
			}
			appState = APP_STATE_IDLE;
			
		} break;

		default:
// Some debugging patches saved here to avoid warning of non-use
// The parameter in the If statement controls how often the state of the output
// pin is changed.  The counters are initialized in the app init function.
debug1Blink++;
if (debug1Blink > 2) {
	debug1Blink = 0;
	HAL_GPIO_debug1_toggle();
}
debug2Blink++;
if (debug2Blink > 1) {
	debug2Blink = 0;
	HAL_GPIO_debug2_toggle();
}
debug3Blink++;
if (debug3Blink > 1) {
	debug3Blink = 0;
	HAL_GPIO_debug3_toggle();
}
debug4Blink++;
if (debug4Blink > 1) {
	debug4Blink = 0;
	HAL_GPIO_debug4_toggle();
}
		break;
	}
}

/*****************************************************************************
		Main Program
*****************************************************************************/
int main(void)
{
// Initialize the system and network function infrastructure
	SYS_Init();

// Then loops indefinitely, alternating between processing system tasks and app tasks
	while (1)
// Execute all of the pending system and network stacks that make everything else work
// Note that this is where callbacks will be executed!
	{
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
