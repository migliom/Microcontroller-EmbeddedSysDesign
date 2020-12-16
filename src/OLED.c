/*
 * OLED.c
 *
 *  Created on: Dec 3, 2020
 *      Author: matteomiglio
 */
#include "stm32f0xx.h"
#include "lcd.h"
#include <stdio.h> // for sprintf()
void small_delay(){
	nano_wait(1000000);
}
void spi_data(int hooligan){
	for(;;){
		if((SPI2->SR & SPI_SR_TXE) == SPI_SR_TXE)
		{
			SPI2->DR = (hooligan | (0x200));
			break;
		}
	}

}
void spi_display1(const char *displayChar){
	spi_cmd(0x02); // move the cursor to the lower row (offset 0x40)
	int i = 0;
	while(displayChar[i] != '\0'){
		spi_data((displayChar[i]));
		++i;
	}
}
void spi_display2(const char *displayChar){
	spi_cmd(0xc0); // move the cursor to the lower row (offset 0x40)
	int i = 0;
	while(displayChar[i] != '\0'){
		spi_data((displayChar[i]));
		++i;
	}
}
void bb_data(int data){
	GPIOB->ODR &= ~(1<<12);
	small_delay();
	bb_write_bit(1);
	bb_write_bit(0);
	bb_write_byte(data);
	small_delay();
	//GPIOB->ODR &= ~(1<<12);
	GPIOB->ODR |= (1<<12);
	small_delay();
}

void spi_cmd(int koh){
	for(;;){
		if((SPI2->SR & SPI_SR_TXE) == SPI_SR_TXE)
		{
			SPI2->DR = (koh);
			break;
		}
	}

}
void bb_write_bit(int bit){
	GPIOB->ODR &= ~(1<<15);
	GPIOB->ODR |= (bit << 15);
	small_delay();
	GPIOB->ODR &= ~(1<<13);
	GPIOB->ODR |= (1 << 13);
	small_delay();
	GPIOB->ODR &= ~(1<<13);
}
void bb_write_byte(int byte1){
	for(int i = 7; i >= 0; i--)
	{
		bb_write_bit((byte1>>i)&0x1);
	}
}
void bb_cmd(int cmd){
	GPIOB->ODR &= ~(1<<12);
	small_delay();
	bb_write_bit(0);
	bb_write_bit(0);
	bb_write_byte(cmd);
	small_delay();
	//GPIOB->ODR &= ~(1<<12);
	GPIOB->ODR |= (1<<12);
	small_delay();
}
void spi_init_oled(){
	nano_wait(1000000);
	spi_cmd(0x38);
	spi_cmd(0x08);
	spi_cmd(0x01);
	nano_wait(2000000);
	spi_cmd(0x06);
	spi_cmd(0x02);
	spi_cmd(0x0c);
}

void setup_spi2(){
	RCC->AHBENR |= RCC_AHBENR_GPIOBEN;
	GPIOB->MODER &= ~(0xcf000000);
	GPIOB->MODER |= 0x8a000000;
	RCC->APB1ENR |= RCC_APB1ENR_SPI2EN;
	SPI2->CR1 |= SPI_CR1_MSTR | SPI_CR1_BR;
	SPI2->CR1 |= SPI_CR1_BIDIMODE | SPI_CR1_BIDIOE;
	SPI2->CR2 = SPI_CR2_DS_3 |SPI_CR2_DS_0 | SPI_CR2_SSOE | SPI_CR2_NSSP;;
	SPI2->CR1 |= SPI_CR1_SPE;
}
