#include <stdio.h>
#include <stdint.h>
#include <util/delay.h>
#include <avr/io.h>
#include "avr_common/uart.h" // this includes the printf and initializes it

#define CPU_FREQ 16000000UL

uint16_t do_adc() {
	//setting registers
	ADCSRA |= 1<<6;								//start conversion
	uint16_t res = 0;
	if ((ADCSRA & 1<<4) && !(ADCSRA && 1<<6)) {
		res = ADCL;
		res += ADCH >> 9;
	}
	return res;
}

int main(void){
	//initialize printf/uart
	printf_init();

	//setting registers
	ADMUX |= 1<<6;						//setting Vref = AVcc = 5V, using pin Analog in #A0 in single mode
	ADCSRA |= 1<<7; 					//enabling the ADC
	ADCSRA |= 1<<2 | 1<<1 | 1<<0;		//setting prescaler at 128 -> 16M/128 = 125kHz clock frequency for the ADC
	
	uint16_t i;
	while(1) {
		i = do_adc();
		printf("Current value: %d A\n", i);
		_delay_ms(100);					//wait 0.1 sec
	} 
}