#include <stdio.h>
#include <stdint.h>
#include <avr/io.h>
#include <util/delay.h>
#include "avr_common/uart.h"			//serial printf

#define DELAY 500						//delay[ms]

#define MINH 60							//minutes in an hour
#define HIND 24							//hours in a day
#define DINM 30							//days in a month
#define MINY 12							//months in a year

uint16_t stats_hour[MINH];
uint16_t stats_day[HIND];
uint16_t stats_month[DINM];
uint16_t stats_year[MINY];
uint8_t index_h, index_d, index_m, index_y;

#define MSMINUTE 60000UL				//ms in a min
#define MSHOUR 3600000UL				//ms in an hour
#define MSDAY 86400000UL				//ms in a day
#define MSMONTH 2592000000UL			//ms in a month (30 days)

uint64_t time;

uint16_t do_adc() {
	//setting registers
	uint16_t res = 0;
	ADCSRA |= (1<<6);						//start conversion
	res = ADCL;
	res |= (ADCH<< 8);
	//res is now a value between 0 and 1023 (0 = GND, 1023 = Vref = 5V)
	printf("res = %d\n", res);
	//res = Vin * 1023 / Vref -> Vin = res / 1023 * Vref
	float vin = ((float)res / 1023.0f) * 5.0f;
	//input current is between 0 and 10A 
	uint16_t i = vin * 10 * 1000 / 5 / 50;		//in mA
	return i;
}

//stats (save on a file)
void sample_hour(uint16_t i) {
	if (index_h >= MINH)
		index_h = 0;
	stats_hour[index_h++] = i;
}

void sample_day(uint16_t i) {
	if (index_d >= HIND)
		index_d = 0;
	stats_day[index_d++] = i;
}

void sample_month(uint16_t i) {
	if (index_m >= DINM)
		index_m = 0;
	stats_month[index_m++] = i;
}

void sample_year(uint16_t i) {
	if (index_y >= MINY)
		index_y = 0;
	stats_year[index_y++] = i;
}

int main(void){
	//initialize printf/uart
	printf_init();

	ADMUX |= (1<<6);						//setting Vref = AVcc = 5V, using pin Analog in #A0
	ADCSRA |= (1<<7); 					//enabling the ADC
	ADCSRA |= (1<<2) | (1<<1) | (1<<0);		//setting prescaler at 128 -> 16M/128 = 125kHz clock frequency for the ADC
	
	uint16_t i;
	while(1) {
		i = do_adc();
		printf("Current value: %u mA\n", i);
		
		if (time % MSMINUTE == 0) {
			sample_hour(i);
			if (time % MSHOUR == 0) {
				sample_day(i);
				if (time % MSDAY == 0) {
					sample_month(i);
					if (time % MSMONTH == 0)
						sample_year(i);
				}
			}
		}

		_delay_ms(DELAY);
		time += DELAY;
	}
}