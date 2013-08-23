/*
 * \file ModuleTest.c
 *
 * \brief App framework for testing new boards
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
	NWK_STATE_INITIAL,
	NWK_STATE_IDLE,
	NWK_STATE_WAITSEND,
	NWK_STATE_RECD
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

extern void updateLEDs (uint8_t colorArray[], uint16_t numLEDs);
extern void outPortE (uint8_t diagInfo);

static void appSendAddr(void);


/*****************************************************************************
		Variables
*****************************************************************************/

static uint16_t myAddr;

#ifndef FIXED_ADDR
static uint16_t networkAddr EEMEM;
#endif

static AppState_t appState = APP_STATE_INITIAL;
static AppState_t nwkState = NWK_STATE_INITIAL;
static SYS_Timer_t appTimer;
static NWK_DataReq_t appDataReq;
static bool appDataReqBusy = false;
static uint8_t appDataReqBuffer[APP_BUFFER_SIZE];
static uint8_t appWorkingBuffer[APP_BUFFER_SIZE];
static uint8_t appWorkingBufferPtr = 0;

static LED_Command_t *cmdBuffer;
static uint8_t LEDarray[NUM_LEDS*3];
static uint8_t currentLEDmode;
static uint8_t latestRSSI;

static uint16_t mainLoopBlink;
static uint16_t debug1Blink;
static uint16_t debug2Blink;
static uint16_t debug3Blink;
static uint16_t debug4Blink;

/*****************************************************************************
		Function implementations
*****************************************************************************/

static void appAddrConf(NWK_DataReq_t *req)
{
	appDataReqBusy = false;
    debug2Blink++;
}

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
static void appTimerHandler(SYS_Timer_t *timer)
{
	updateLEDs(LEDarray, NUM_LEDS*3);
	for (int LED_ptr=0;LED_ptr<NUM_LEDS*3;LED_ptr+=3)
	{
		LEDarray[LED_ptr]+=4; 			// Green
		LEDarray[LED_ptr+1]+=4;			// Red
		LEDarray[LED_ptr+2]+=4;			// Blue
	}
//	if (appState == APP_STATE_WAITSEND)	appSendData();
//	(void)timer;
}

/*****************************************************************************
	Callback function from the network stack for app endpoint LED Command
*****************************************************************************/
static bool LEDCmdDataInd(NWK_DataInd_t *ind)
{
	memcpy(appWorkingBuffer, ind->data, ind->size);
	cmdBuffer = appWorkingBuffer;
	nwkState = NWK_STATE_RECD;
	appState = APP_STATE_DATARDY;
	
	HAL_GPIO_statusLED_toggle();

	latestRSSI = ind->rssi;

	return true;
}

/*****************************************************************************
	Callback function from the network stack for energy detection measurement
*****************************************************************************/
void PHY_EdConf(int8_t energyLevel)
{
	currentLEDmode = currentLEDmode;
}

/*****************************************************************************
	Startup configuration
*****************************************************************************/
static void appInit(void)
{
#ifndef FIXED_ADDR
	eeprom_busy_wait();
	ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {	
		eeprom_write_word(&networkAddr, 5);
		myAddr = eeprom_read_word(&networkAddr);
	}
#else
	myAddr = APP_ADDR;
#endif
	NWK_SetAddr(myAddr);
	NWK_SetPanId(APP_PANID);
	PHY_SetChannel(APP_CHANNEL);
	PHY_EdReq();
	PHY_SetRxState(true);
	NWK_OpenEndpoint(LEDCmd_ENDPOINT, LEDCmdDataInd);

	appTimer.interval = APP_FLUSH_TIMER_INTERVAL;
	appTimer.mode = SYS_TIMER_PERIODIC_MODE;
	appTimer.handler = appTimerHandler;
	SYS_TimerStart(&appTimer);

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

	debug1Blink = 0;
	debug2Blink = 0;
	debug3Blink = 0;
	debug4Blink = 0;

	currentLEDmode = LED_MODE_IDLE;
	for (int LED_ptr=0;LED_ptr<NUM_LEDS*3;LED_ptr+=3)
	{
		LEDarray[LED_ptr] = 85;				// Green
		LEDarray[LED_ptr+1] = 170;			// Red
		LEDarray[LED_ptr+2] = 255;			// Blue
//		LEDarray[LED_ptr] = 0xC3;				// Green
//		LEDarray[LED_ptr+1] = 0x99;				// Red
//		LEDarray[LED_ptr+2] = 0x0F;				// Blue
	}
	updateLEDs(LEDarray, NUM_LEDS*3);

	appDataReqBusy = false;
	
// Broadcast this mote's address
	appWorkingBufferPtr = 2;
	appWorkingBuffer[0] = myAddr;
	appWorkingBuffer[1] = 0;
	appSendAddr();
}

/*****************************************************************************
		Task Handler
*****************************************************************************/
static void APP_TaskHandler(void)
{
  // Put your application code here	
    switch (appState)
	{
		case APP_STATE_INITIAL:
		{
			appInit();
			appState = APP_STATE_IDLE;
		} break;

		case APP_STATE_IDLE:
		break;

		case APP_STATE_DATARDY:
		{
			if (cmdBuffer->mode == MODE_GLOBAL)
			{
				if(cmdBuffer->subMode == STATIC)
				{
					currentLEDmode = LED_MODE_FIXED;
//					Fixed color mode where command provides color values
/*
					for (int LED_ptr=0;LED_ptr<(NUM_LEDS-2);LED_ptr+=3)
					{
						LEDarray[LED_ptr] = cmdBuffer->grnIntensity;		// Green
						LEDarray[LED_ptr+1] = cmdBuffer->redIntensity;		// Red
						LEDarray[LED_ptr+2] = cmdBuffer->bluIntensity;		// Blue
					}
					updateLEDs(LEDarray, NUM_LEDS*3);
*/
					appState = APP_STATE_IDLE;
				} else if(cmdBuffer->subMode == RANDOM)
				{
					currentLEDmode = LED_MODE_RANDOM;
//					Random color mode where command provides color values
					updateLEDs(LEDarray, NUM_LEDS*3);
					
				}
				
			} else if (cmdBuffer->mode == MODE_PEER_TO_PEER)
			{
//					updateLEDs(LEDarray, NUM_LEDS*3);

			} else
			{
// Some debugging patches saved here to avoid warning of non-use
    debug1Blink++;
    if (debug1Blink > 25000) {
	    debug1Blink = 0;
	    HAL_GPIO_debug1_toggle();
    }
    debug4Blink++;
    if (debug4Blink > 1) {
	    debug4Blink = 0;
	    HAL_GPIO_debug4_toggle();
    }
    if (debug2Blink > 1) {
	    debug2Blink = 0;
	    HAL_GPIO_debug2_toggle();
    }
    debug3Blink++;
    if (debug3Blink > 1) {
	    debug3Blink = 0;
	    HAL_GPIO_debug3_toggle();
    }


			}
			appState = APP_STATE_IDLE;
			
		} break;

		default:
		break;
	}
}

/*****************************************************************************
		Main Program
*****************************************************************************/
int main(void)
{
  SYS_Init();

// Loops indefinitely, alternating between processing system tasks and app tasks

  while (1)
  {
    SYS_TaskHandler();
//	HAL_GPIO_hbLED_toggle();
    APP_TaskHandler();
    mainLoopBlink++;
    if (mainLoopBlink > 25000) {
	    mainLoopBlink = 0;
	    HAL_GPIO_hbLED_toggle();
	    HAL_GPIO_statusLED_toggle();
    }
	outPortE(0xFF);
	outPortE(0x00);
  }
}
