#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
#include <avr/interrupt.h>
#include <avr/iom2560.h>
#include "avr_common/uart.h"			//serial printf and getchar
#include <util/delay.h>

#define CPV 1024						//conversions per value
uint16_t adjust = 0;

#define MINH 60							//minutes in an hour
#define HIND 24							//hours in a day
#define DINM 30							//days in a month
#define MINY 12							//months in a year

volatile uint64_t time = 0;				//volatile: it is updated by an ISR
ISR(TIMER1_COMPA_vect) {
	time += 100;
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
uint16_t delay = 1000;					//1s default delay
//last sample time for on-line mode
uint64_t lsto = 0;

#define MSMINUTE 60000UL				//ms in a min
#define MSHOUR 3600000UL				//ms in an hour
#define MSDAY 86400000UL				//ms in a day
#define MSMONTH 2592000000UL			//ms in a month (30 days)

uint16_t max = 0;

void timer1_A_init() {
	//CTC mode (Clear Timer on Compare Match), 64 prescaler
	TCCR1B |= (1 << WGM12) | TCCR1B | (1 << CS11) | (1 << CS10);
	//interrupt enabled for compare match with channel A
	TIMSK1 |= (1 << OCIE1A);
	//match frequency = 16MHz / (64 * (OCR1A + 1)) = 10Hz -> every 0.1s
	OCR1A = 24999;
	//enable global interrupts
	sei();
}
//analog-digital conversion
uint16_t do_adc() {
	uint16_t res, res_max = 0, res_min = 1023;
	double v_in, v_ref = 5.f, v_bias = 2.5f, v_rms, sample = 0;
	for (int j = 0; j < CPV; j++) {
		//start conversion
		ADCSRA |= (1 << ADSC);
		res = ADCL;
		res |= (ADCH << 8);
		if (res > res_max) res_max = res;
		if (res < res_min) res_min = res;
		//0 <= res <= 1023 (0 = GND, 1023 = Vref = 5V)
		//because of v_bias = 2.5V, when no current is flowing ADC result is approximately 512
		//res = (v_in + v_bias) * 1023 / v_ref -> v_in = res / 1023 * v_ref - v_bias
		v_in = (float)(res + adjust) / 1023.f * v_ref - v_bias;
		sample += v_in *  v_in;
	}
	//adjustment to avoid drift
	if ((res_max - res_min) < 10){ 
		adjust = 512 - (res_max + res_min) / 2;
		return 0;
	}
	v_rms = sqrt(sample / CPV);
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
		//reject newline
		if (command == '\n') return;

		//on-line mode
		if (command == 'o') {
			//little delay to wait other character and avoid processing just the 'o'
			_delay_ms(3);
			if (usart_kbhit()) {
				char delay_char = usart_getchar();
				if (delay_char >= '1' && delay_char <= '9') {
					//in ms
					delay = (delay_char - '0') * 1000;
					online_mode = 1;
					printf("on-line mode enabled, sampling every %d second(s)\n", delay / 1000);
				} else {
					printf("ERROR: use oX with 1 <= X <= 9\n");
				}
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
		}
		//hour stats
		else if (command == 'h') {
			printf("last hour stats (every minute):\n");
			for (int i = 0; i < MINH; i++) {
				printf("minute #%d: %u mA\n", i, stats_hour[i]);
			}
		}
		//day stats
		else if (command == 'd') {
			printf("last day stats (every hour):\n");
			for (int i = 0; i < HIND; i++) {
				printf("hour #%d: %u mA\n", i + 1, stats_day[i]);
			}
		}
		//month stats
		else if (command == 'm') {
			printf("last month stats (every day):\n");
			for (int i = 0; i < DINM; i++) {
				printf("day #%d: %u mA\n", i + 1, stats_month[i]);
			}
		}
		//year stats
		else if (command == 'y') {
			printf("last year stats (every month):\n");
			for (int i = 0; i < MINY; i++) {
				printf("month #%d: %u mA\n", i + 1, stats_year[i]);
			}
		}
		//highest value
		else if (command == 'x') {
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
		//list commands
		else if (command == 'l') {
			printf("  oX = enable on-line mode, sampling every 1 <= X <= 9 seconds\n  h = last hour (60 minutes)\n  d = last day (24 hours)\n  m = last month (30 days)\n  y = last year (12 months)\n  c = clear statistics\n  x = display maximum value sampled until now\n");
		}
		//incorrect input
		else {
			printf("ERROR: unknown command. Send l to list commands\n");
		}
	}
}

int main(void){
	//initialize timer #1 for comparison with channel A
	timer1_A_init();
	uint64_t current_time = 0;
	//initialize printf/uart
	printf_init();
	printf("Welcome in Arduino current meter! Available commands:\n");
	printf("  oX = enable on-line mode, sampling every 1 <= X <= 9 seconds\n  h = last hour (60 minutes)\n  d = last day (24 hours)\n  m = last month (30 days)\n  y = last year (12 months)\n  c = clear statistics\n  x = display maximum value sampled until now\n");
	
	//setting Vref = 5V, using pin Analog in #A0
	ADMUX |= (1 << REFS0);
	//enabling the ADC
	ADCSRA |= (1 << ADEN);
	//setting prescaler at 128 -> 16M/128 = 125kHz clock frequency for the ADC
	ADCSRA |= (1 << ADPS2) | (1 << ADPS1) | (1 << ADPS0);

	//a first conversion to set the adjust value and stabilize future conversions
	do_adc();
	uint16_t i = 0;
	uint64_t elapsed_time;
	while(1) {
		if (online_mode && (time - lsto >= delay)) {
			i = do_adc();
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
			i = do_adc();
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

		//commands from serial (cutecom)
		get_command();
	}
}
