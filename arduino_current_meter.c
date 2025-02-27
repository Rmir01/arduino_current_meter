#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <math.h>
#include <avr/io.h>
#include <avr/iom2560.h>
#include <util/delay.h>
#include "avr_common/uart.h"			//serial printf

#define CPV 1024

#define MINH 60							//minutes in an hour
#define HIND 24							//hours in a day
#define DINM 30							//days in a month
#define MINY 12							//months in a year

volatile uint64_t time = 0;				//volatile: it is updated by an ISR
ISR(TIMER1_COMPA_vect) {
	time +=100;
}

uint16_t stats_hour[MINH];
uint16_t stats_day[HIND];
uint16_t stats_month[DINM];
uint16_t stats_year[MINY];
uint8_t index_h = 0, index_d = 0, index_m = 0, index_y = 0;
//last sample time h/d/m/y
uint64_t lsth = 0, lstd = 0, lstm = 0, lsty = 0;

//on-line mode global variables
uint8_t online_mode = 0;
uint16_t delay = 1000;					//1s
//last sample time for on-line mode
uint64_t lsto = 0;

#define MSMINUTE 60000UL				//ms in a min
#define MSHOUR 3600000UL				//ms in an hour
#define MSDAY 86400000UL				//ms in a day
#define MSMONTH 2592000000UL			//ms in a month (30 days)

uint16_t max = 0;

void timer1_A_init() {
	//CTC mode (Clear Timer on Compare Match)
	TCCR1B |= (1 << WGM12);
	//interrupt enabled for compare match with channel A
	TIMSK1 |= (1 << OCIE1A);
	//prescaler = 64
	TCCR1B |= (1 << CS11) | (1 << CS10);
	//match frequency = 16MHz / (2 * 64 * (OCR1A + 1)) = 10Hz -> every 0.1s
	OCR1A = 12499;
	//enable global interrupts
	sei();
}
//analog-digital conversion
uint16_t do_adc() {
	uint16_t res = 0;
	double v_in, v_rms, sample = 0;
	for (int j = 0; j < CPV; j++) {
		//start conversion
		ADCSRA |= (1 << ADSC);
		while (ADCSRA & (1 << ADSC));
		res = ADCL;
		res |= (ADCH << 8);
		//0 <= res <= 1023 (0 = GND, 1023 = Vref = 5V)
		//because of v_bias = 2.5V, when no current is flowing ADC result is approximately 512
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

//stats
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
	lstd = MSHOUR;
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

//serial interface to view and query the data
void get_command() {
	if (usart_kbhit()) {
		char command = usart_getchar();

		//on-line mode
		if (command == 'o') {
			char delay_char = usart_getchar();
			if (delay_char >= '1' && delay_char <= '9') {
				//in ms
				delay = (delay_char - '0') * 1000;
				online_mode = 1;
				printf("on-line mode enabled, sampling every %d seconds\n", delay / 1000);
			} else {
				printf("ERROR: use oX with 1 <= X <= 9\n");
			}
		} else if (command == 's') {
			if (online_mode) {
				online_mode = 0;
				printf("exited on-line mode!\n");
			} else {
				printf("ERROR: not in on-line mode!\n");
			}
		} else if (command == 'h') {
			printf("last hour stats (every minute):\n");
			for (int i = 0; i < MINH; i++) {
				printf("minute #%d: %u mA\n", i, stats_hour[i]);
			}
		} else if (command == 'd') {
			printf("last day stats (every hour):\n");
			for (int i = 0; i < HIND; i++) {
				printf("hour #%d: %u mA\n", i, stats_day[i]);
			}
		} else if (command == 'm') {
			printf("last month stats (every day):\n");
			for (int i = 0; i < DINM; i++) {
				printf("day #%d: %u mA\n", i, stats_month[i]);
			}
		} else if (command == 'y') {
			printf("last year stats (every month):\n");
			for (int i = 0; i < MINY; i++) {
				printf("month #%d: %u mA\n", i, stats_year[i]);
			}
		} else if (command == 'x') {
			printf("highest current value sampled: %dmA\n", max);
		}		
		//clear statistics
		else if (command == 'c') {
			memset(stats_hour, 0, sizeof(stats_hour));
			memset(stats_day, 0, sizeof(stats_day));
			memset(stats_month, 0, sizeof(stats_month));
			memset(stats_year, 0, sizeof(stats_year));
			printf("stats have been made shine!\n");
		} 
		else {
			printf("ERROR: unknown command, please use:\n");
			printf("  oX = enable on-line mode, sampling every 1 <= X <= 9 seconds\n  h = last hour (60 minutes)\n  d = last day (24 hours)\n  m = last month (30 days)\n  y = last year (12 months)\n  c = clear statistics\n  x = display maximum value sampled until now\n");
		}
	}
}

int main(void){
	//initialize timer #1 for comparison with channel A
	uint64_t current_time = time;
	timer1_A_init();
	//initialize printf/uart
	printf_init();
	
	//setting Vref = 5V, using pin Analog in #A0
	ADMUX |= (1 << REFS0);
	//enabling the ADC
	ADCSRA |= (1 << ADEN);
	//setting prescaler at 128 -> 16M/128 = 125kHz clock frequency for the ADC
	ADCSRA |= (1 << ADPS2) | (1 << ADPS1) | (1 << ADPS0);
	
	uint16_t i = 0;
	uint64_t elapsed_time;
	while(1) {
		//TO REMOVE: test with high rate output
		i = do_adc();
		printf("Current value: %u mA\n", i);
		///////////////////////////////////////
		if (online_mode && (time - lsto >= delay)) {
			//TO KEEP: for on-line mode
			//i = do_adc();
			if (i > max) max = i;
			printf("on-line mode: current is now %umA\n", i);
			lsto = time;
        }
		
		elapsed_time = time - current_time;
		current_time = time;
		lsth += elapsed_time;
		lstd += elapsed_time;
		lstm += elapsed_time;
		lsty += elapsed_time;
		//sample if time is more than min/hour/day/month
		if (lsth >= MSMINUTE) {
			//TO KEEP: for stats
			//i = do_adc();
			if (i > max) max = i;
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

		//commands from serial
		get_command();
	}
}
//https://docs.google.com/document/d/1MCLB4Y08Cot9-LnUW7ijtXNUZDSUrg2T6U3W_ST-UtA/edit?tab=t.0