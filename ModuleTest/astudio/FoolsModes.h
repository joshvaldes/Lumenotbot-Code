/*
	Constants, types, and structures used for the Fool Lanterns
*/
/*
typedef enum transforms_t
{
	STATIC, ROTATE, FLASH, RANDOM, THROB, FIRECRACKER, ORBITALS
} transforms_t;
*/
typedef struct LED_Command_t {
	enum		{MODE_GLOBAL, MODE_PEER_TO_PEER} mode;
	enum		{STATIC, ROTATE, FLASH, RANDOM, THROB, FIRECRACKER, ORBITALS} subMode;
//	transforms_t	transform;
	uint8_t		redIntensity[NUM_LEDS];		// Red value for all LEDs
	uint8_t		grnIntensity[NUM_LEDS];		// Green value for all LEDs
	uint8_t		bluIntensity[NUM_LEDS];		// Blue value for all LEDs
	uint8_t		period_mS;					// mS
	uint8_t		modeParam;					// extra parameter specific to mode
} LED_Command_t;

// App endpoints
#define LEDCmd_ENDPOINT				1
#define Mote_Addr_ENDPOINT			16

#define BROADCAST_ADDR				0xFFFF