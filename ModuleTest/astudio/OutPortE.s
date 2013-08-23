#include <avr/io.h>
.global outPortE

//Use r18-r27, r30, r31
//Don't use r2-r17, r28, r29

outPortE:
			ldi		r18,0xFF
			out		0x0D,r18
			out		0x0E,r24
			ret