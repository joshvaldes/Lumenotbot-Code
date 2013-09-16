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
	enum		{MODE_GLOBAL, MODE_PEER_TO_PEER, MODE_NOCHANGE} mode;
	enum		{STATIC, ROTATE, FLASH, RANDOM, THROB, FIRECRACKER, ORBITALS, ONESHOT} subMode;
//	transforms_t	transform;
	uint8_t		redIntensity[NUM_LEDS];		// Red value for all LEDs
	uint8_t		grnIntensity[NUM_LEDS];		// Green value for all LEDs
	uint8_t		bluIntensity[NUM_LEDS];		// Blue value for all LEDs
	uint16_t	modeParam;					// extra parameter specific to mode
	uint32_t	period_mS;					// mS
} LED_Command_t;

// App endpoints
#define LEDCmd_ENDPOINT				1
#define SyncCmd_ENDPOINT			2
#define AddrCheck_ENDPOINT			3
#define Mote_Addr_ENDPOINT			4

#define BROADCAST_ADDR				0xFFFF