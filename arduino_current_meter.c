#include <stdio.h>
#include <stdint.h>
#include "avr_common/uart.h"			//serial printf and getchar
#include "modules.h"

uint64_t lsth = 0, lstd = 0, lstm = 0, lsty = 0;

//last sample time for on-line mode
uint64_t lsto = 0;

int main(void){
	//initialize timer #1 for comparison with channel A
	timer1_A_init();
	uint64_t current_time = 0;
	//initialize printf/uart
	printf_init();
	printf("Welcome in Arduino current meter! Available commands:\n");
	printf("  oX = enable on-line mode, sampling every 1 <= X <= 9 seconds\n  h = last hour stats (60 minutes)\n  d = last day stats (24 hours)\n  m = last month stats (30 days)\n  y = last year stats (12 months)\n  c = clear statistics\n  max = maximum value sampled until now\n  q to quit\n");
	//initialize ADC
	adc_init();
	
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
		//commands from client
		get_command();
	}
}
