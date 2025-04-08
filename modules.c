#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
#include <avr/interrupt.h>
#include <avr/iom2560.h>
#include <util/delay.h>
#include "avr_common/uart.h"		//serial printf
#include "modules.h"

#define CPV 1024					//conversions per value
uint16_t adjust = 0;				//value that sets ADC's result at 512 when current = 0
uint16_t max = 0;

#define MINH 60						//minutes in an hour
#define HIND 24						//hours in a day
#define DINM 30						//days in a month
#define MINY 12						//months in a year

#define CMD_MAX_SIZE 64

uint16_t stats_hour[MINH];
uint16_t stats_day[HIND];
uint16_t stats_month[DINM];
uint16_t stats_year[MINY];
uint8_t index_h = 0, index_d = 0, index_m = 0, index_y = 0;

//ISR for timer1
volatile uint64_t time = 0;
ISR(TIMER1_COMPA_vect) {
	time += 100;
}

//ISR for serial communication
volatile uint8_t cmd[CMD_MAX_SIZE];
volatile uint8_t index_c = 0;
volatile uint8_t cmd_ready = 0;
ISR(USART0_RX_vect) {
	char c = UDR0;
	if (c == '\0' || c == '\n' || c == '\r') {
		cmd_ready = 1;
		cmd[++index_c] = '\0';
	}
	else
		cmd[index_c++] = c;
}

//on-line mode 0/1
uint8_t online_mode = 0;
uint16_t delay;

//timer
void timer1_A_init() {
	//CTC mode (Clear Timer on Compare match), 64 prescaler
	TCCR1B |= (1 << WGM12) | TCCR1B | (1 << CS11) | (1 << CS10);
	//disable interrupts during execution of this section
	cli();
	//interrupt enabled for compare match with channel A
	TIMSK1 |= (1 << OCIE1A);
	//enable interrupts
	sei();
	//match frequency = 16MHz / (64 * (OCR1A + 1)) = 10Hz -> every 0.1s
	OCR1A = 24999;
}

//analog-digital conversion
void adc_init() {
	//setting Vref = 5V, using pin Analog in #A0
	ADMUX |= (1 << REFS0);
	//enabling the ADC
	ADCSRA |= (1 << ADEN);
	//setting prescaler at 128 -> 16M/128 = 125kHz clock frequency for the ADC
	ADCSRA |= (1 << ADPS2) | (1 << ADPS1) | (1 << ADPS0);
}

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
		//because of v_bias = 2.5V, when no current is flowing ADC result should be 512
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
	if (!cmd_ready) return;
	//printf("command received: %s\n", cmd);
	//on-line mode
	if (cmd[0] == 'o') {
		char delay_char = cmd[1];
		if (cmd[2] == '\0' && delay_char >= '1' && delay_char <= '9') {
			//in ms
			delay = (delay_char - '0') * 1000;
			online_mode = 1;
			printf("on-line mode enabled, sampling every %d second(s)\n", delay / 1000);
		} else {
			printf("ERROR: use oX with 1 <= X <= 9\n");
		}
	}
	//highest value
	else if (cmd[0] == 'm' && cmd[1] == 'a' && cmd[2] == 'x' && cmd[3] == '\0') {
		printf("highest current sampled: %dmA\n", max);
		}
	else if (cmd[1] == '\0') {
		if (cmd[0] == 's') {
			if (online_mode) {
				online_mode = 0;
				printf("exited on-line mode!\n");
			} else {
				printf("ERROR: not in on-line mode!\n");
			}
		}
		//hour stats
		else if (cmd[0] == 'h') {
			printf("last hour stats (every minute):\n");
			for (int i = 0; i < MINH; i++) {
				printf("minute #%d: %u mA  ", i, stats_hour[i]);
				if ((i + 1) % 6 == 0)
					printf("\n");
			}
		}
		//day stats
		else if (cmd[0] == 'd') {
			printf("last day stats (every hour):\n");
			for (int i = 0; i < HIND; i++) {
				printf("hour #%d: %u mA  ", i + 1, stats_day[i]);
				if ((i + 1) % 6 == 0)
					printf("\n");
			}
		}
		//month stats
		else if (cmd[0] == 'm') {
			printf("last month stats (every day):\n");
			for (int i = 0; i < DINM; i++) {
				printf("day #%d: %u mA  ", i + 1, stats_month[i]);
				if ((i + 1) % 6 == 0)
					printf("\n");
			}
		}
		//year stats
		else if (cmd[0] == 'y') {
			printf("last year stats (every month):\n");
			for (int i = 0; i < MINY; i++) {
				printf("month #%d: %u mA  ", i + 1, stats_year[i]);
				if ((i + 1) % 6 == 0)
					printf("\n");
			}
		}		
		//clear statistics
		else if (cmd[0] == 'c') {
			memset(stats_hour, 0, sizeof(stats_hour));
			memset(stats_day, 0, sizeof(stats_day));
			memset(stats_month, 0, sizeof(stats_month));
			memset(stats_year, 0, sizeof(stats_year));
			printf("stats have been made shine!\n");
		} 
		//list commands
		else if (cmd[0] == 'l')
			printf("  oX = enable on-line mode, sampling every 1 <= X <= 9 seconds\n  h = last hour stats (60 minutes)\n  d = last day stats (24 hours)\n  m = last month stats (30 days)\n  y = last year stats (12 months)\n  c = clear statistics\n  max = maximum value sampled until now\n  q to quit\n");
		//incorrect input
		else
			printf("ERROR: unknown command. Send l to list commands\n");
	}
	//incorrect input
	else {
		printf("ERROR: unknown command. Send l to list commands\n");
	}
	for (int i = 0; i < CMD_MAX_SIZE; i++) {
		cmd[i] = 0;
	}
	cmd_ready = 0;
	index_c = 0;
}
