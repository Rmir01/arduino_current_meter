#pragma once

//macros
#define MSMINUTE 60000UL				//ms in a min
#define MSHOUR 3600000UL				//ms in an hour
#define MSDAY 86400000UL				//ms in a day
#define MSMONTH 2592000000UL			//ms in a month (30 days)

//ADC
uint16_t do_adc(void);
void adc_init(void);

//timer
extern volatile uint64_t time;          //volatile: it is updated by an ISR
void timer1_A_init(void);

//stats
extern uint64_t lsth, lstd, lstm, lsty; //last sample time h/d/m/y
void sample_hour(uint16_t i);
void sample_day(uint16_t i);
void sample_month(uint16_t i);
void sample_year(uint16_t i);

//commands
extern uint8_t online_mode;
extern uint16_t delay;
extern uint16_t max;				//highest current value sampled
void get_command(void);
