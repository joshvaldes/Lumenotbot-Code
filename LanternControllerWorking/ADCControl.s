/*
 * Assembly1.s
 *
 * Created: 4/4/2013 12:04:33 PM
 *  Author: Skaterdude
 */
//Use r18-r27, r30, r31
//Don't use r2-r17, r28, r29
//#define __AVR_ATmega128RFA1__
//#define __ASSEMBLER__
#include <avr/io.h>
.global InitADC
.global GetADC
#define ADCH 0x79	// To get rid of warning

// Initialize the ADC infrastructure
InitADC:
	ldi		r18, (1<<REFS0)+(1<<REFS1)+(1<<ADLAR)	// Initial setting for MUX
	sts		ADMUX, r18
	ldi		r18,(1<<ADEN)+(1<<ADPS0)	//enable the ADC, prescaler = 2
	sts		ADCSRA, r18
	ldi		r18,0x1F
	sts		DIDR0, r18
wait_avdd_ok:						//wait for AVDD to come up
	lds		r18, ADCSRB
	sbrs	r18, AVDDOK
	rjmp	wait_avdd_ok
// set start-up time to 16us (max possible with 8MHz ADC clock)
	ldi		r18, 0x1F
	sts		ADCSRC, r18
	ret

// Read one of the ADC channels and return an 8-bit value
// Channel is provided as a parameter
GetADC:
	ldi		r18, 0
	sts		ADCSRB, r18				// set MUX5 before MUX4:1
// 1.6V reference voltage
	ldi		r18, (1<<REFS0)+(1<<REFS1)+(1<<ADLAR)
	andi	r24, 0x07
	or		r18, r24
	sts		ADMUX, r18
wait_vref_ok:
	lds		r18, ADCSRB				// wait for reference voltage
	sbrs	r18, REFOK
	rjmp	wait_vref_ok
	ldi		r18, (1<<ADEN)+(1<<ADSC)+(1<<ADPS0)
// Start conversion
	sts		ADCSRA, r18
wait_adsc:
	lds		r18, ADCSRA
	sbrc	r18, ADSC				// flag cleared at conversion complete
	rjmp	wait_adsc
	lds		r24, ADCH				// return result
	ldi		r25, 0
	ret
