#include <stdio.h>
#include <stdint.h>
#include <math.h>
#include <avr/io.h>
#include <util/delay.h>
#include "avr_common/uart.h"			//serial printf

#define DELAY 100						//delay[ms]
#define CPV 1024

#define MINH 60							//minutes in an hour
#define HIND 24							//hours in a day
#define DINM 30							//days in a month
#define MINY 12							//months in a year

uint16_t stats_hour[MINH];
uint16_t stats_day[HIND];
uint16_t stats_month[DINM];
uint16_t stats_year[MINY];
uint8_t index_h, index_d, index_m, index_y;

#define MSMINUTE 60000UL					//ms in a min
#define MSHOUR 3600000UL					//ms in an hour
#define MSDAY 86400000UL					//ms in a day
#define MSMONTH 2592000000UL				//ms in a month (30 days)

uint64_t lsth, lstd, lstm, lsty;			//last sample time h/d/m/y

uint16_t do_adc() {
	uint16_t res = 0;
	double v_in, v_rms, sample = 0;
	for (int j = 0; j < CPV; j++) {
		//start conversion
		ADCSRA |= (1 << 6);
		while (ADCSRA & (1 << 6));
		res = ADCL;
		res |= (ADCH << 8);
		//res is now a value between 0 and 1023 (0 = GND, 1023 = Vref = 5V)
		//when no current is flowing, ADC result is approximately 512 (2.5V = v_bias)
		//res = (v_in + v_bias) * 1023 / v_ref -> v_in = res / 1023 * v_ref - v_bias
		v_in = (float)res / 1023.f * 5.f - 2.5f;
		sample += v_in *  v_in;
	}
	v_rms = sqrt(sample / CPV);
	//printf("res = %d\n", res);
	//0.185 A/V, result in mA
	uint16_t i_rms = round(v_rms / 0.185f * 1000.f);
	return i_rms;
}

//stats (save on a file?)
void sample_hour(uint16_t i) {
	if (index_h >= MINH)
		index_h = 0;
	stats_hour[index_h++] = i;
	lsth -= MSMINUTE;
}

void sample_day(uint16_t i) {
	if (index_d >= HIND)
		index_d = 0;
	stats_day[index_d++] = i;
	lstd -= MSHOUR;
}

void sample_month(uint16_t i) {
	if (index_m >= DINM)
		index_m = 0;
	stats_month[index_m++] = i;
	lstm -= MSDAY;
}

void sample_year(uint16_t i) {
	if (index_y >= MINY)
		index_y = 0;
	stats_year[index_y++] = i;
	lsty -= MSMONTH;
}

void first_sample(uint16_t i) {
	sample_hour(i);
	sample_day(i);
	sample_month(i);
	sample_year(i);
}

int main(void){
	//initialize printf/uart
	printf_init();

	//setting Vref = 5V, using pin Analog in #A0
	ADMUX |= (1 << 6);
	//enabling the ADC
	ADCSRA |= (1 << 7);
	//setting prescaler at 128 -> 16M/128 = 125kHz clock frequency for the ADC
	ADCSRA |= (1 << 2) | (1 << 1) | (1 << 0);
	

	uint16_t i;
	i = do_adc();
	first_sample(i);
	uint64_t time = 0;
	while(1) {
		i = do_adc();
		printf("Current value: %u mA\n", i);
		//time from last conversion
		time = millis() - time;
		//sample if time is more than min/hour/day/month
		lsth += time;
		lstd += time;
		lstm += time;
		lsty += time;
		if (lsth >= MSMINUTE) {
			sample_hour(i);
			if (lstd >= MSHOUR) {
				sample_day(i);
				if (lstm >= MSDAY) {
					sample_month(i);
					if (lsty >= MSMONTH) {
						sample_year(i);
					}
				}
			}
		}

		_delay_ms(DELAY);
	}
}
//https://docs.google.com/document/d/1MCLB4Y08Cot9-LnUW7ijtXNUZDSUrg2T6U3W_ST-UtA/edit?tab=t.0