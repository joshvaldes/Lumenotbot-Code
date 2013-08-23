#include <avr/io.h>
.global updateLEDs

// r18-r27, r30, r31

updateLEDs:
#define PORTD 0x0B
#define PD4 4
			mov		r30, r24				//1 Get the pointer to the array of bytes
			mov		r31, r25				// Put it into the Z register (R30/R31)
			mov		r28, r22				// Load the outer loop counter
			mov		r29, r23				// Load the outer loop counter
			ld		r26, Z+					// Get the first byte and increment the array pointer
			in 		r27, 0x3F				// Save status register

			cli								// Turn off interrupts
/**************************************************************************************/
ByteLoop:
			ldi 	r20, 8					//1 Load/reload bit count
/* Loop over 8 bits */
BitLoop:
			sbi  	PORTD,PD4				//2 Output high
			sbrc 	r26, 7					//1/2 Check if upper bit is 0 or 1
			rjmp 	Bit1					//2 For 0, skip jump and do first bit clear
			cbi  	PORTD,PD4				//2 
Bit1:										//For 1, clear output to low later
			sbrc 	r26, 7					//1/2 Check if upper bit is 0 or 1
			rjmp 	Here					//2 For 0, skip jump and do first bit clear
Here:
			sbiw 	r28, 1					//2 Decrement outer loop counter
			breq 	Exit					//1/2 exit outer loop if zero
			adiw 	r28, 1					//2 Bump back up outer loop counter
			nop								//1 62.5 nS delay
			lsl 	r26						//1 Move next bit in
			dec 	r20						//1 Decrement inner loop counter
			cbi 	PORTD,PD4				//2 Clear output to low
			brne 	BitLoop					//1/2 Send next bit
			ld 		r26, Z+					//2 load in next byte
			sbiw 	r28, 1					//2 Decrement outer loop counter
			rjmp 	ByteLoop				//2 Restart loop with new byte
/**************************************************************************************/
Exit:
			nop								//1 62.5 nS delay
			cbi 	PORTD,PD4				// Make sure output is low
			sei								// Turn interrupts back on
			out 	0x3F, r27				// Restore the status register
			ret
