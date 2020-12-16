/*
 * music.c
 *
 *  Created on: Dec 1, 2020
 *      Author: matteomiglio
 */
#include "stm32f0xx.h"
#include <stdint.h>
#include <stdlib.h>
#include "lcd.h"
#include <string.h> // for memset() declaration
#include <math.h>

#define N 1000
#define RATE 20000
int volume = 2048;
int stepa = 0;
int offseta = 0;
int offset = 0;
int sevenSegOff = 0;
short int wavetable[N];
int songNum = 1;
int scoreOffset = 0;
struct {
	float frequency;
	uint16_t duration;
} song[] = {
            { 523.25, 1000 }, // C5
            { 587.33, 1000 }, // D5
            { 659.25, 1000 }, // E5
            { 698.46, 1000 }, // F5
            { 783.99, 1000 }, // G5
            { 880.00, 1000 }, // A6
            { 987.77, 1000 }, // B6
            { 1046.50, 1000}, // C6
};

struct {
	float frequency;
	uint16_t duration;
} song1[] = {
            { 523.25, 1000 }, // C5
            { 587.33, 1000 }, // D5
            { 659.25, 1000 }, // E5
            { 1046.50, 1000 }, // F5
            { 783.99, 1000 }, // G5
            { 880.00, 1000 }, // A6
            { 987.77, 1000 }, // B6
            { 698.46, 1000}, // C6
};
char display[8];
const char font[] = {
        [' '] = 0x00,
        ['0'] = 0x3f,
        ['1'] = 0x06,
        ['2'] = 0x5b,
        ['3'] = 0x4f,
        ['4'] = 0x66,
        ['5'] = 0x6d,
        ['6'] = 0x7d,
        ['7'] = 0x07,
        ['8'] = 0x7f,
        ['9'] = 0x67,
        ['A'] = 0x77,
        ['B'] = 0x7c,
        ['C'] = 0x39,
        ['D'] = 0x5e,
        ['*'] = 0x49,
        ['#'] = 0x76,
        ['.'] = 0x80,
        ['?'] = 0x53,
        ['b'] = 0x7c,
        ['r'] = 0x50,
        ['g'] = 0x6f,
        ['i'] = 0x10,
        ['n'] = 0x54,
        ['u'] = 0x1c,
		['F'] = 0x71,
		['H'] = 0x76,
		['E'] = 0x79,
		['Y'] = 0x6e,
		['o'] = 0x5c,
};
void enable_ports()
{
	/*configure PC0 – PC10 to be outputs,
	configure PB0 – PB3 to be outputs,
	configure PB4 – PB7 to be inputs, and
	configure internal pull-down resistors for PB4 – PB7*/
	RCC->AHBENR |= (0x00040000)| (0x00080000);
	GPIOC->MODER &= ~(0x2fffff);
	GPIOC->MODER |= 0x155555; //PC0-PC10

	GPIOB->MODER &= ~(0xffff); //PB4-PB7 inputs and clear PB0-PB3 bits
	GPIOB->MODER |= 0x55; //PB0-PB3 outputs
	GPIOB->PUPDR &= ~(0xffff);
	GPIOB->PUPDR |= 0xaa00;
}
void update7(int seg){
	sevenSegOff = seg;
}
void show_digit(int _offset)
{
	//copy lab5 show digit
	/*Read the 3-bit value from offset and set that value on pins PC[10:8]
	Look up the array element display[offset] and output that value to PC[7:0]*/
	int off = sevenSegOff & 7;
	GPIOC->ODR = (off << 8) | display[off];
}
//add another song struct and ask user which sonng they wanna listen to, but end up using the same song
void init_wavetable(void)
{
	for(int i=0; i < N; i++)
		wavetable[i] = 32767 * sin(2 * M_PI * i / N);
}
void setup_tim1()
{
	RCC->AHBENR |= RCC_AHBENR_GPIOAEN;
	GPIOA->MODER |= 0xaa0000;
	//GPIOA->MODER |= 0x800000;
	GPIOA->AFR[1] &= ~(0xf << (4*(8-8))); //clear the bits for pin 8
	GPIOA->AFR[1] |= (0x2 << (4*(8-8))); //Alternate function 2
	GPIOA->AFR[1] &= ~(0xf << (4*(9-8))); //clear the bits pin 9
	GPIOA->AFR[1] |= (0x2 << (4*(9-8))); //Alternate function 2
	GPIOA->AFR[1] &= ~(0xf << (4*(10-8))); //clear the bits pin 10
	GPIOA->AFR[1] |= (0x2 << (4*(10-8))); //Alternate function 2
	GPIOA->AFR[1] &= ~(0xf << (4*(11-8))); //clear the bits pin 11
	GPIOA->AFR[1] |= (0x2 << (4*(11-8))); //Alternate function 2

	RCC->APB2ENR |= RCC_APB2ENR_TIM1EN;
	TIM1->BDTR |= 0x8000; //MOE bit is in the 15th bit position of TIM1_BDTR, write a 1 to it to enable

	TIM1->PSC = 1-1;
	TIM1->ARR = 2400-1;
	//PWM = 110
	TIM1->CCMR1 &= ~TIM_CCMR1_OC1M_0; //turn off bit 0
	TIM1->CCMR1 &= ~TIM_CCMR1_OC2M_0; //turn off bit 0 (I think we want 011)
	TIM1->CCMR1 |= TIM_CCMR1_OC1M_1 | TIM_CCMR1_OC1M_2; //enable bit 2 and bit 1
	TIM1->CCMR1 |= TIM_CCMR1_OC2M_1 | TIM_CCMR1_OC2M_2;

	TIM1->CCMR2 &= ~TIM_CCMR2_OC3M_0;
	TIM1->CCMR2 &= ~TIM_CCMR2_OC4M_0;
	TIM1->CCMR2 |= TIM_CCMR2_OC3M_1 | TIM_CCMR2_OC3M_2;
	TIM1->CCMR2 |= TIM_CCMR2_OC4M_1 | TIM_CCMR2_OC4M_2;

	TIM1->CCMR2 |=  TIM_CCMR2_OC4PE;//0x800;//Configure the CCMR2 register to set the "preload enable" bit only for channel 4.

	TIM1->CCER |= TIM_CCER_CC1E | TIM_CCER_CC2E | TIM_CCER_CC3E | TIM_CCER_CC4E;

	TIM1->CR1 |= TIM_CR1_CEN;

}

void set_freq_a(float f)
{
	if(f == 0.0){
		stepa = 0;
		offseta = 0;
	}
	else
		stepa = f * N/RATE * (1<<16);
}


void setup_tim7(int _songNum)
{
	RCC->APB1ENR |= (1<<5);
	//RCC->APB1ENR &= ~(1<<4);

	TIM7->PSC = 48000-1;
	TIM7->ARR = 1000-1;

	TIM7->DIER |= (1<<0);

	TIM7->CR1 |= (1<<0);

	NVIC->ISER[0] = (1 << 18);
	songNum = _songNum;

}
void goodbye7Seg(){
	memset(display, 0, 8);
	display[7] = font['u'];
	display[6] = font['o'];
	display[5] = font['Y'];
	display[3] = font[' '];
	display[2] = font['E'];
	display[1] = font['E'];
	display[0] = font['5'];
}
void updateScore(int score){
	memset(display, 0, 8);
	display[7] = font['n'];
	display[6] = font['u'];
	display[5] = font['F'];
	display[3] = font['E'];
	display[2] = font['u'];
	display[1] = font['A'];
	display[0] = font['H'];
}
void TIM7_IRQHandler(){
	TIM7->SR &= ~(TIM_SR_UIF);
	offset = (offset + 1) & 0x7; // count 0 ... 7 and repeat
	set_freq_a(song[offset].frequency);
}

void setup_tim6(void)
{
	RCC->APB1ENR |= (1<<4);
	TIM6->PSC = 240-1;
	TIM6->ARR = 10-1;
	TIM6->DIER =0x1;
	TIM6->CR1 = 0x1;
	NVIC->ISER[0] = (1 << 17);
}

void TIM6_DAC_IRQHandler(void) {
	/*Acknowledge the timer interrupt by writing a zero to the UIF bit of the timer's status register.
	-Trigger the DAC. Do this first to avoid variable-delay calculations, below, from introducing any jitter into the signal.
	-Add stepn to offsetn.
	-If any of the offsetn variables are greater than or equal to the maximum fixed-point value, subtract the maximum fixed-point value from it.
	-Look up the four samples at each of the four offsets in the wavetable array and add them together into a single combined sample variable.
	-Reduce and shift the combined sample to the range of the DAC by dividing it by 32 and adding 2048.
	-If the adjusted sample is greater than 4095, set it to 4095. (Clip a too-high value.)
	-If the adjusted sample is less than 0, set it to 0. (Clip a too-low value.)
	Write the final adjusted sample to the DAC's DHR12R1 register.*/
	TIM6->SR &= ~TIM_SR_UIF;
	//DAC->SWTRIGR |= DAC_SWTRIGR_SWTRIG1;
	offseta = offseta + stepa;
	        if ((offseta>>16) >= N)
	            offseta -= N<<16;

	int totalSample = wavetable[offseta>>16];
	totalSample = ((totalSample * volume) >> 17) + 1200;
	//totalSample > 4095 ? 4095 : totalSample;
	if(totalSample > 4095) {totalSample = 4095;}
	if(totalSample < 0) {totalSample = 0;}
	TIM1->CCR4 = totalSample;
}

void setrgb(int where)
{
	int rgb;
	if(where == 1) //touch paddle
		rgb = 0x000022;
	if(where == 2)
		rgb = 0x002200;
	if(where == 3)
		rgb = 0x220000;
	unsigned int red = ((rgb & 0xF00000) >> 20) * 10 + ((rgb & 0xF0000) >> 16);
	int redCCR1 = (1-(red/100.0)) * (TIM1->ARR + 1);
	if(redCCR1 % 2)
		redCCR1++;

	unsigned int green = ((rgb & 0xF000) >> 12) * 10 + ((rgb & 0xF00) >> 8);
	int greenCCR2 = (1-(green/100.0)) * (TIM1->ARR + 1);
	if(greenCCR2 % 2)
		greenCCR2++;

	unsigned int blue = ((rgb & 0xF0) >> 4) * 10 + ((rgb & 0xF) >> 0);
	int blueCCR3 = (1-(blue/100.0)) * (TIM1->ARR + 1);
	if(blueCCR3 % 2)
		blueCCR3++;
	TIM1->CCR1 = redCCR1;
	TIM1->CCR2 = greenCCR2;
	TIM1->CCR3 = blueCCR3;
}
