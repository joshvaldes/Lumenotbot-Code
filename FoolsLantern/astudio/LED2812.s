#include <avr/io.h>
.global updateLEDs

//Use r18-r27, r30, r31
//Don't use r2-r17, r28, r29
/*	This function receives a pointer to an array of LED intensity=ies in green / red / blue order,
	along with the length in bytes.  It outputs the bits of the array in the format defined for
	the WS2811 LED driver and WS2812 LED with integrated driver.  Timing is written for 800kHz
	bit rate with 250nS on time for zeroes and 1000nS (1 uS) on time for ones.  This function is
	time-critical, so interrupts are turned off while it runs and restored upon exit.

	The address of the array of bytes is expected in registers 24 and 25.  The number of bytes
	in the form of a two-byte integer is expected in registers 22 and 23.  Note that this is the
	number of bytes, not the number of LEDs, as there are 3 color intensity bytes per LED.
		
	Note that a 16 MHz processor clock / instruction executing rate is assumed.  The number at
	the start of most comments is the number of clock cycles required to execute that instruction.
	Branches have two outcomes: more instruction cycles if the branch is taken and fewer if not.
	This feature is used to equalize the timing between 0 and 1 bit sequences.  If you count the
	instruction eecution cycles, assume that the digital output pins do not change until the
	end of the instruction, which is after the second instruction cycle.  This really does not
	matter as long as you are consistent in the way you treat the instruction timing.

	To Do:  Fix the fact that an extra bit is sent out because the exit branch is taken on the
	first execution of the bit loop for a non-existent byte (off the end of the array).  This
	doesn't really cause any harm unless the function is handed a number of bytes that is smaller
	than the actual number of LEDs (x3 for R, G, and B).
*/
#define BOARD_VERSION 1
// This parameter changes the output pin for the serial data.
#if BOARD_VERSION == 1
// Version 1 will output the serial data on pin 3 of port E
// This pin is on the 8 dual-pin header strip next to the ISP connector
#define PORTD 0x0E
#define PD4 3
#elif BOARD_VERSION == 2
// Version 2 will output the serial data on pin 4 of port D
// this pin is brought out to the S-LED connector along with battery power and ground
#define PORTD 0x0B
#define PD4 4
#else
#endif

updateLEDs:			// Preamble to  load parameters, save state, and prevent interrupts
			mov		r30, r24				// Get the pointer to the array of bytes
			mov		r31, r25				// Put it into the Z register (R30/R31)
			mov		r26, r22				// Load the outer loop counter
			mov		r27, r23				// Load the outer loop counter
			adiw 	r26, 1					// Add one to outer loop count so last bit goes out

			in 		r18, 0x3F				// Save status register
			cli								// Turn off interrupts
//			sbi		0x0e,7
			ld		r19, Z+					// Get the first byte and increment the array pointer
/**************************************************************************************/
ByteLoop:
			ldi 	r20, 8					//1 Load/reload bit count
/* Loop over 8 bits */
BitLoop:
			sbi  	PORTD,PD4				//2 Output high; does not occur until end of instruction
			nop								//1 62.5 nS delay
			sbrc 	r19, 7					//1/2 Check if upper bit is 0 or 1
			rjmp 	Bit1					//2 For 1, this branch is taken (not skipped)
			cbi  	PORTD,PD4				//2 For 0, the previous branch is skipped and this is done
Bit1:										// (For 1, the output is cleared later)
			sbrc 	r19, 7					//1/2 Check if upper bit is 0 or 1
			rjmp 	Here					//2 For 1, this adds an instruction cycle
Here:
			nop								//1 62.5 nS delay
			sbiw 	r26, 1					//2 Decrement outer loop counter
			breq 	Exit					//1/2 exit outer loop if zero
			adiw 	r26, 1					//2 Bump outer loop counter back up
			lsl 	r19						//1 Move next bit in
			dec 	r20						//1 Decrement inner loop counter
			cbi 	PORTD,PD4				//2 Clear output to low
			brne 	BitLoop					//1/2 Send next bit
			ld 		r19, Z+					//2 load in next byte
			sbiw 	r26, 1					//2 Decrement outer loop counter
			rjmp 	ByteLoop				//2 Restart loop with new byte
/**************************************************************************************/
Exit:
			nop								//1 62.5 nS delay
			cbi 	PORTD,PD4				// Make sure output is low
			out 	0x3F, r18				// Restore the status register
//			cbi		0x0e,7
			sei								// Turn interrupts back on
			ret