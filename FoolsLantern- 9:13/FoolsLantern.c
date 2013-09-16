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
	APP_STATE_MESH
} AppState_t;

/*****************************************************************************
Function prototypes
*****************************************************************************/

extern void updateLEDs (uint8_t colorArray[], uint16_t numLEDs);
extern void outPortE (uint8_t diagInfo);

#ifdef DUPL_CHECK
static void appSendAddr(void);
static void appAddrCheckSendData(void);
#endif

/*****************************************************************************
Variables
*****************************************************************************/

static uint16_t myAddr;
static uint16_t storedAddr;

static uint16_t EEMEM APP_EEPROM_ADDR;

static AppState_t appState;
#ifdef DUPL_CHECK
static SYS_Timer_t addrCheckTimer;
#endif
static SYS_Timer_t animationTimer;
static SYS_Timer_t accelerationTimer;
static SYS_Timer_t cmdTimer;
#ifdef SEARCH_CHAN
static SYS_Timer_t channelTimer;
static uint8_t savePattRed;
static uint8_t savePattGrn;
static uint8_t savePattBlu;
#endif
static NWK_DataReq_t appSyncReq;
#ifdef DUPL_CHECK
static NWK_DataReq_t appDataReq;
static uint8_t appDataReqBuffer[APP_BUFFER_SIZE];
#endif
static bool appDataReqBusy = false;
static bool appSyncReqBusy = false;
static bool cmdTimeout;
static uint8_t appWorkingBuffer[APP_BUFFER_SIZE];
static uint8_t appWorkingBufferPtr = 0;

static uint8_t currentChannel;

static LED_Command_t *cmdBuffer;
static int cmdBufferPtr;
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
static uint16_t chanScanLEDPtr;
static uint8_t latestRSSI;
static uint8_t averageRSSI;			//to save previous value, this might need to be global

static uint16_t mainLoopBlink;
static uint16_t debug1Blink;
static uint16_t debug2Blink;
static uint16_t debug3Blink;
static uint16_t debug4Blink;

/*****************************************************************************
Function implementations
*****************************************************************************/
#ifdef DUPL_CHECK
/*****************************************************************************
This is a callback to process the acknowledgement from an address
message sent by this node.  Note that the status is not checked.
*****************************************************************************/
static void appAddrConf(NWK_DataReq_t *req)
{
	appDataReqBusy = false;
}

/*****************************************************************************
This function broadcasts this node's address so that other nodes know it
exists and that this address is taken.
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
If duplicate address checking is enabled in the config.h file, then this
is the timer handler that would send out the request.
*****************************************************************************/
static void addrCheckTimerHandler(SYS_Timer_t *timer)
{
	// Not implemented
}

/*****************************************************************************
Callback function from the network stack for addr check response.  If the
address of this node is already in use, then it changes to another address
which is either random (if the EEPROOM is empty) or the next sequential
address if there is already a node address stored in the EEPROM.
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
			myAddr = eeprom_read_word(&APP_EEPROM_ADDR);
			if ((myAddr == 0) || (myAddr == 0xFFFF))
			{
				//			This would only be executed if the EEPROM location is 0 or not programmed
				myAddr = rand();
			} else
			{
				//			If there is another node using this address on the mesh, just increment the local address.
				//			Eventually this will be checked again for a collision.
				myAddr++;
			}
			// Save the new address in EEPROM
			eeprom_write_word(&APP_EEPROM_ADDR, myAddr);
		}
		// Update the network stack with the new address
		NWK_SetAddr(myAddr);
		#endif
		// Error handling.  How to do this?  TODO
	} else if ((req->status == NWK_NO_ACK_STATUS)||(req->status == NWK_PHY_NO_ACK_STATUS))
	{
	} else
	{
	}
	// Clear the pending request flag
	appDataReqBusy = false;
}
/*****************************************************************************
// The function used to send a check for non-unique address, i.e., see if another
// node is using the same address as this one.
*****************************************************************************/
static void appAddrCheckSendData(void)
{
	if (appDataReqBusy)
	return;

	//TODO
	// Need some kind of key here to distinguish msgs that might get sent to self!
	memcpy(appDataReqBuffer, appWorkingBuffer, sizeof(LED_Command_t));

	appDataReq.dstAddr = myAddr;
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
}
#endif
/*****************************************************************************
This call back processes the return from the request to send out a sync
message that tells other nodes to go into mesh mode (no controller).
*****************************************************************************/
static void appSyncConf(NWK_DataReq_t *req)
{
	appSyncReqBusy = false;
}

/*****************************************************************************
This function sends a sync message, telling other nodes to go into mesh
mode without a controller.  This is a simple state that is triggered by
a switch on one of the nodes so that no controller is required.
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
	
	// Sync mode is preset local accelerating throb
	currentLEDmode = FLASH;
	for (int LED_ptr=0;LED_ptr<NUM_LEDS*3;LED_ptr+=3)
	{
		LEDarray[LED_ptr] = 0;				// Green
		LEDpattern[LED_ptr] = 0;			// Green
		LEDarray[LED_ptr+1] = 32;			// Red
		LEDpattern[LED_ptr+1] = 32;			// Red
		LEDarray[LED_ptr+2] = 0;			// Blue
		LEDpattern[LED_ptr+2] = 0;		// Blue
	}
	animationTimerPeriod = 125;
	animationTimer.interval = animationTimerPeriod;
	// Clear the timeout flag so we don't switch to local mode
	cmdTimeout = false;
	syncOn = false;
	appState = APP_STATE_LOCAL;
}

/*****************************************************************************
Callback function from the network stack for the start / sync command.  If
the sync input on any node in the mesh is active, then that node will send
a sync message to all of the other nodes (broadcast).  This is the handler
for those requests.  It sets the node to THROB mode with color blue.
*****************************************************************************/
static bool SyncDataInd(NWK_DataInd_t *ind)
{
	HAL_GPIO_statusLED_toggle();
	// Turn on the sync command flag
	syncOn = true;
	// Set the mode to locked to command node
	appState = APP_STATE_MESH;
	// Sync mode is preset to local accelerating throb
	currentLEDmode = FLASH;
	flashState = 0;
	for (int LED_ptr=0;LED_ptr<NUM_LEDS*3;LED_ptr+=3)
	{
		LEDarray[LED_ptr] = 0;				// Green
		LEDpattern[LED_ptr] = 0;			// Green
		LEDarray[LED_ptr+1] = 32;			// Red
		LEDpattern[LED_ptr+1] = 32;			// Red
		LEDarray[LED_ptr+2] = 0;			// Blue
		LEDpattern[LED_ptr+2] = 0;			// Blue
	}
	return true;
}
/*****************************************************************************
This function handles changing the THROB mode brightness change amount.
Because the LED brightness is non-linear, we might want to change the
delta as the brightness grows or fades using the acceleration timer.
*****************************************************************************/
static void accelerationTimerHandler(SYS_Timer_t *timer)
{
	if (currentLEDmode == THROB)
	{
		if (throbTimerAccel != 0)		// Update the throb period if accelerating
		{
			if (animationTimerPeriod > 50) // Minimum update period is 50 mS
			{
				animationTimerPeriod -= throbTimerAccel;
				if (animationTimerPeriod < 50)
				animationTimerPeriod = 50;
				animationTimer.interval = animationTimerPeriod;
			} else if (throbDelta < 5)		// Maximum delta is 5
			{
				throbDelta++;
			}
		}
	}
}
#ifdef SEARCH_CHAN
/*****************************************************************************
Callback function from the timer subsystem.  The timer is set to periodically
invoke this function when scanning for a channel with a controller on it.
As long as there is no command timeout, this timer will be stopped.  Once a
timeout occurs, the node starts scanning the channels, looking for a control
node sending LED commands.
*****************************************************************************/
static void channelTimerHandler(SYS_Timer_t *timer)
{
	channelTimer.interval = CHANNEL_SCAN_INTERVAL;
	// Start by switching to the next channel
	currentChannel++;
	if (currentChannel > HIGH_CHANNEL)
	currentChannel = LOW_CHANNEL;
	PHY_SetChannel(currentChannel);
	// Restore the one just completed
	LEDpattern[chanScanLEDPtr] = savePattGrn;
	LEDpattern[chanScanLEDPtr+1] = savePattRed;
	LEDpattern[chanScanLEDPtr+2] = savePattBlu;
	// Then update the pointer
	chanScanLEDPtr = (currentChannel-LOW_CHANNEL)*3;
	// Make the current one white
	savePattGrn = LEDpattern[chanScanLEDPtr];
	savePattRed = LEDpattern[chanScanLEDPtr+1];
	savePattBlu = LEDpattern[chanScanLEDPtr+2];
	LEDpattern[chanScanLEDPtr] = 32;
	LEDpattern[chanScanLEDPtr+1] = 32;
	LEDpattern[chanScanLEDPtr+2] = 32;
	HAL_GPIO_debug1_toggle();
}
#endif
/*****************************************************************************
Callback function from the timer subsystem.  The timer is set to periodically
invoke this function to update the LED pattern.
*****************************************************************************/
static void cmdTimerHandler(SYS_Timer_t *timer)
{
	// If the cmdTimeout flag is set when this timer goes off, it means that no commands
	// have been received since the last time.  In that case, this node switches into the
	// default local mode.  The channel timer is started so that this node scans all of the
	// network radio channels, looking for a controller node that is sending commands.
	if (cmdTimeout && (appState != APP_STATE_LOCAL))
	{
		appState = APP_STATE_LOCAL;
		//		Put the LEDs into blue throb mode
		currentLEDmode = FLASH;
		flashState = 0;
		for (int LED_ptr=0;LED_ptr<NUM_LEDS*3;LED_ptr+=3)
		{
			LEDarray[LED_ptr] = 0;				// Green
			LEDpattern[LED_ptr] = 0;			// Green
			LEDarray[LED_ptr+1] = 16;			// Red
			LEDpattern[LED_ptr+1] = 16;			// Red
			LEDarray[LED_ptr+2] = 0;			// Blue
			LEDpattern[LED_ptr+2] = 0;			// Blue
		}
		animationTimerPeriod = 125;
		animationTimer.interval = animationTimerPeriod;
		#ifdef SEARCH_CHAN
		currentChannel = HIGH_CHANNEL;
		channelTimer.interval = 500;			// Make sure scanning starts quickly
		SYS_TimerStart(&channelTimer);
		HAL_GPIO_debug4_set();
		// Calc the pointer to the LED corresponding to the current radio channel
		chanScanLEDPtr = (currentChannel-LOW_CHANNEL)*3;
		// Save LED brightness for current channel
		savePattGrn = LEDpattern[chanScanLEDPtr];
		savePattRed = LEDpattern[chanScanLEDPtr+1];
		savePattBlu = LEDpattern[chanScanLEDPtr+2];
		#endif
		// Otherwise, the flag was reset by a command that was received.  In this case, the flag
		// is set again to see if a command is received in the next timer interval.  So the node
		// is periodically checking to see that some command has been received from another node in
		// the mesh.  As long as something is going on, it will not drop into the local default mode.
	} else if (!cmdTimeout)
	{
		cmdTimeout = true;
	}
}
/*****************************************************************************
Callback function from the timer subsystem.  The timer is set to periodically
invoke this function to update the LED pattern.
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
Callback function from the network stack for the LED Command app endpoint.
Messages that carry commands go through this function
*****************************************************************************/
static bool LEDCmdDataInd(NWK_DataInd_t *ind)
{
	appState = APP_STATE_DATARDY;
	// Clear the timeout flag so we don't switch to local mode
	cmdTimeout = false;
	#ifdef SEARCH_CHAN
	SYS_TimerStop(&channelTimer);
	HAL_GPIO_debug4_clr();
	#endif
	// Make sure the pointer is set correctly
	cmdBuffer = &appWorkingBuffer[0];
	// Copy the data from the message buffer into the command buffer so that the
	// network buffer can be freed up and re-used
	memcpy(appWorkingBuffer, ind->data, ind->size);
	// This is the received signal strength value which might be useful	for some
	// swarm rule(s)
	latestRSSI = (ind->rssi + 90) * 9;
	averageRSSI = (averageRSSI>>2)*3 + latestRSSI;
	// Returning "true" to the network stack says that this message should be acknowledged
	// if the sender requested it.
	HAL_GPIO_rcvLED_toggle();
	return true;
}

/*****************************************************************************
Initialize all of the application-related states
*****************************************************************************/
static void appInit(void)
{
	// This allows the local node address to come from either EEPROM or the config.h file
	#ifdef FIXED_ADDR
	myAddr = APP_ADDR;
	ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
		storedAddr = eeprom_read_word(&APP_EEPROM_ADDR);
		if ((storedAddr = 0x0000) || (storedAddr == 0xFFFF))
		{
			eeprom_write_word(&APP_EEPROM_ADDR, myAddr);
		}
	}
	#else
	eeprom_busy_wait();
	ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
		myAddr = eeprom_read_word(&APP_EEPROM_ADDR);
	}
	#endif
	// Set the seed for the random number generator using the local address
	srand(myAddr);
	// Set up the system and network for the application
	NWK_SetAddr(myAddr);
	NWK_SetPanId(APP_PANID);
	// Start with a default channel in the 2.4GHz band
	currentChannel = APP_CHANNEL;
	PHY_SetChannel(currentChannel);
	// Put radio into receive state
	PHY_SetRxState(true);
	// Instantiate process endpoint for LED output messages
	NWK_OpenEndpoint(LEDCmd_ENDPOINT, LEDCmdDataInd);
	// Instantiate process endpoint for sync messages from other nodes
	NWK_OpenEndpoint(SyncCmd_ENDPOINT, SyncDataInd);
	// Implement a periodic timer to check for duplicate addresses in the mesh
	#ifdef DUPL_CHECK
	addrCheckTimer.interval = ADDR_CHECK_INTERVAL;
	addrCheckTimer.mode = SYS_TIMER_PERIODIC_MODE;
	addrCheckTimer.handler = addrCheckTimerHandler;
	SYS_TimerStart(&addrCheckTimer);
	#endif
	// Implement a timer to determine when to switch to local mode if
	// no commands are received.
	cmdTimer.interval = COMMAND_TIMEOUT_INTERVAL;
	cmdTimer.mode = SYS_TIMER_PERIODIC_MODE;
	cmdTimer.handler = cmdTimerHandler;
	SYS_TimerStart(&cmdTimer);
	// Implement a timer that determines how often the LEDs are changed
	// if there is a pattern that flashes, rotates, etc.  Note that the
	// interval is only initialized here; it can be changed on-the-fly by
	// the application.  For example, the LED command contains an update
	// interval for exactly this purpose.
	animationTimerPeriod = LED_ANIMATION_INTERVAL;
	animationTimer.interval = LED_ANIMATION_INTERVAL;
	animationTimer.mode = SYS_TIMER_PERIODIC_MODE;
	animationTimer.handler = appLEDAnimationTimerHandler;
	SYS_TimerStart(&animationTimer);
	// Implement a timer to increase / decrease the LED update interval for throb mode
	accelerationTimer.interval = ACCELERATION_INTERVAL;
	accelerationTimer.mode = SYS_TIMER_PERIODIC_MODE;
	accelerationTimer.handler = accelerationTimerHandler;
	#ifdef SEARCH_CHAN
	// Implement the timer to determine the time between channels when scanning
	// for a controller.
	channelTimer.interval = CHANNEL_SCAN_INTERVAL;
	channelTimer.mode = SYS_TIMER_PERIODIC_MODE;
	channelTimer.handler = channelTimerHandler;
	//	SYS_TimerStart(&channelTimer);		// Leave this timer stopped to start out
	#endif
	// Initialize the direction and state of all of the outputs
	// Output for LED serial data stream
	HAL_GPIO_lightStripData_out();
	HAL_GPIO_lightStripData_clr();
	// Heartbeat LEDs
	HAL_GPIO_hbLED_out();
	HAL_GPIO_hbLED_clr();
	HAL_GPIO_hbLED1_out();
	HAL_GPIO_hbLED1_clr();
	// Status outputs
	HAL_GPIO_statusLED_out();
	HAL_GPIO_statusLED_clr();
	HAL_GPIO_rcvLED_out();
	HAL_GPIO_rcvLED_clr();
	// Debugging outputs
	HAL_GPIO_debug1_out();
	HAL_GPIO_debug1_clr();
	HAL_GPIO_debug2_out();
	HAL_GPIO_debug2_clr();
	HAL_GPIO_debug3_out();
	HAL_GPIO_debug3_clr();
	HAL_GPIO_debug4_out();
	HAL_GPIO_debug4_clr();
	// Sync input to trigger anarchy mesh
	HAL_GPIO_syncInput_in();
	HAL_GPIO_syncInput_pullup();
	// These counters will be used to make the LEDs more visible to an observer when
	// they flash very fast. This will make the outputs blink, rather than just
	// appear dimmer or brighter.
	debug1Blink = 0;
	debug2Blink = 0;
	debug3Blink = 0;
	debug4Blink = 0;
	// Initialize the animation variables / parameters.  That way if a parameter is not provided
	// via the command message, there is a default value for it.
	//		Default to static / fixed mode to start
	currentLEDmode = STATIC;
	cmdTimeout = true;
	//		For the random mode(s)
	randomFreq = 512;
	//		For the throb mode
	throbDelta = 4;
	// Initialize the LED string to off
	for (int LED_ptr=0;LED_ptr<NUM_LEDS*3;LED_ptr+=3)
	{
		LEDarray[LED_ptr] = 0;			// Green
		LEDarray[LED_ptr+1] = 0;		// Red
		LEDarray[LED_ptr+2] = 0;		// Blue
	}
	updateLEDs(LEDarray, NUM_LEDS*3);
	syncOn = false;
	// Initialize the state of the flag that says the network is processing the last request
	appDataReqBusy = false;
	#ifdef DUPL_CHECK
	// Broadcast this mote's address
	appWorkingBufferPtr = 2;
	appWorkingBuffer[0] = APP_ADDR;
	appWorkingBuffer[1] = 0;
	appSendAddr();
	#endif
	// Initialize the buffer length
	appWorkingBufferPtr = 0;
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
		//			Check if the sync input is grounded (normally pulled-up)
		if (!HAL_GPIO_syncInput_read())
		{
			appSendSync();
		}
		break;
		// This is the default local mode for when there is no command node.  The node runs whatever LED mode
		// has been set and scans the radio channels, looking for another node that is sending commands.
		case APP_STATE_LOCAL:
		{
			//			Check if the sync input is grounded (normally pulled-up)
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
				//				Stop the throbTimerAccel when switching modes
				SYS_TimerStop(&accelerationTimer);
				switch (currentLEDmode)
				{
					case FLASH:
					{
						flashState = 0;
					}	break;
					case RANDOM:
					{
						randomFreq = cmdBuffer->modeParam;
					}	break;
					case THROB:
					{
						throbDelta = cmdBuffer->modeParam;
						throbFade = 1;
						throbTimerAccel = 0;
						//						This is the only mode that needs this timer to run
						SYS_TimerStart(&accelerationTimer);
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

				//				This mode includes a pattern for the LEDs, so copy that into the pattern array
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
				//				Set the LEDs to the desired pattern
				updateLEDs(LEDarray, NUM_LEDS*3);
				appState = APP_STATE_IDLE;
				HAL_GPIO_debug3_toggle();
				// This is for the non-centrally controlled operation.  It should be the default when the node
				// starts up and does not get any communications from a controller.
			} else if (cmdBuffer->mode == MODE_PEER_TO_PEER)
			{
				//					updateLEDs(LEDarray, NUM_LEDS);
				// This is used to clear the command timeout when no command change is necessary
			} else if (cmdBuffer->mode == MODE_NOCHANGE)
			{
				// Restore the one just completed
				if (chanScanLEDPtr)
				{
					LEDpattern[chanScanLEDPtr] = savePattGrn;
					LEDpattern[chanScanLEDPtr+1] = savePattRed;
					LEDpattern[chanScanLEDPtr+2] = savePattBlu;
					chanScanLEDPtr = NULL;
					HAL_GPIO_debug2_toggle();
				}
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
	appState = APP_STATE_INITIAL;
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
			HAL_GPIO_hbLED1_toggle();
		}
	}
}
