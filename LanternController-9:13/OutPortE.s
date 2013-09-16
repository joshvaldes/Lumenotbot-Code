#include <avr/io.h>
.global outPortE

//Use r18-r27, r30, r31
//Don't use r2-r17, r28, r29
/*	This is a simple function to set the output pins on Port E.

	The value to be set on Port E is provided as an integer in register 24 and 25, with the low byte
	in register 24, which is what is needed.  No status is changed and interrupts will not affect the
	operation of this function, so interrupts are left on, if they are already enabled.
		
*/

outPortE:
			ldi		r18,0xFF
			out		0x0D,r18
			out		0x0E,r24
			ret