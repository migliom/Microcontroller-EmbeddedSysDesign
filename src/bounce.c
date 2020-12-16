
#include "stm32f0xx.h"
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include "lcd.h"
#include "fifo.h"
#include "tty.h"
int score = 0;
int highScore = 0;
int restartVal = 0;
char globalKey = 0;
int offset7 = 0;
int difficulty = 1;
void setup_tim17(int diff){
	int arr = 48;
	RCC->APB2ENR |= RCC_APB2ENR_TIM17EN;
	switch(diff){
		case 1:
			break;
		case 2:
			arr = 24;
			break;
		case 3:
			arr = 12;
			break;
	}
	TIM17->PSC = 10000-1;
	TIM17->ARR = arr-1; //100 times per second
	TIM17->DIER |= TIM_DIER_UIE;
	TIM17->CR1 |= TIM_CR1_CEN;
	NVIC->ISER[0] = (1<<22);
}
void setup_tim2(){
	RCC->APB1ENR |= RCC_APB1RSTR_TIM2RST;
	TIM2->PSC = 10-1;
	TIM2->ARR = 48-1; //100 times per second
	TIM2->DIER |= TIM_DIER_UIE;
	TIM2->CR1 |= TIM_CR1_CEN;
	NVIC->ISER[0] = (1<<15);
}
void TIM2_IRQHandler(void)
{
    TIM2->SR &= ~TIM_SR_UIF;
    update7(offset7);
    offset7 = (++offset7) & 0x7;
    show_digit();
}
void setup_portb()
{
    RCC->AHBENR |= RCC_AHBENR_GPIOBEN;// Enable Port B.
    GPIOB->MODER &= ~(0xffff);
    GPIOB->MODER |= 0x55;// Set PB0-PB3 for output.
    GPIOB->PUPDR |= 0xaa00;// Set PB4-PB7 for input and enable pull-down resistors.
    GPIOB->BSRR = 0x8; // Turn on the output for the lower row.
}

char check_key()
{
    int check = (GPIOB->IDR & 0x10);// If the '*' key is pressed, return '*'
    if(check) {return 'D';}
    check = (GPIOB->IDR & 0x80);
    if(check) {return '*';}
    return 0;
    // If the 'D' key is pressed, return 'D'
    // Otherwise, return 0
}

void setup_spi1()
{
	RCC->AHBENR |= RCC_AHBENR_GPIOAEN;
	GPIOA->MODER &= ~(0xfff0);
	GPIOA->MODER |= 0x8a50;
	RCC->APB2ENR |= RCC_APB2ENR_SPI1EN;
	SPI1->CR1 |= SPI_CR1_MSTR;
	SPI1->CR1 &= ~(SPI_CR1_BR_0);
	SPI1->CR1 &= ~(SPI_CR1_BR_1);
	SPI1->CR1 &= ~(SPI_CR1_BR_2);
	SPI1->CR1 |= SPI_CR1_BIDIMODE | SPI_CR1_BIDIOE;
	SPI1->CR2 = SPI_CR2_DS_2 | SPI_CR2_DS_1 |SPI_CR2_DS_0 | SPI_CR2_SSOE | SPI_CR2_NSSP;;
	SPI1->CR1 |= SPI_CR1_SPE;
}

// Copy a subset of a large source picture into a smaller destination.
// sx,sy are the offset into the source picture.
void pic_subset(Picture *dst, const Picture *src, int sx, int sy)
{
    int dw = dst->width;
    int dh = dst->height;
    if (dw + sx > src->width)
        for(;;)
            ;
    if (dh + sy > src->height)
        for(;;)
            ;
    for(int y=0; y<dh; y++)
        for(int x=0; x<dw; x++)
            dst->pix2[dw * y + x] = src->pix2[src->width * (y+sy) + x + sx];
}

// Overlay a picture onto a destination picture.
// xoffset,yoffset are the offset into the destination picture that the
// source picture is placed.
// Any pixel in the source that is the 'transparent' color will not be
// copied.  This defines a color in the source that can be used as a
// transparent color.
void pic_overlay(Picture *dst, int xoffset, int yoffset, const Picture *src, int transparent)
{
    for(int y=0; y<src->height; y++) {
        int dy = y+yoffset;
        if (dy < 0 || dy >= dst->height)
            continue;
        for(int x=0; x<src->width; x++) {
            int dx = x+xoffset;
            if (dx < 0 || dx >= dst->width)
                continue;
            unsigned short int p = src->pix2[y*src->width + x];
            if (p != transparent)
                dst->pix2[dy*dst->width + dx] = p;
        }
    }
}

// Called after a bounce, update the x,y velocity components in a
// pseudo random way.  (+/- 1)
void perturb(int *vx, int *vy)
{
    if (*vx > 0) {
        *vx += (random()%3) - 1;
        if (*vx >= 3)
            *vx = 2;
        if (*vx == 0)
            *vx = 1;
    } else {
        *vx += (random()%3) - 1;
        if (*vx <= -3)
            *vx = -2;
        if (*vx == 0)
            *vx = -1;
    }
    if (*vy > 0) {
        *vy += (random()%3) - 1;
        if (*vy >= 3)
            *vy = 2;
        if (*vy == 0)
            *vy = 1;
    } else {
        *vy += (random()%3) - 1;
        if (*vy <= -3)
            *vy = -2;
        if (*vy == 0)
            *vy = -1;
    }
}

extern const Picture background; // A 240x320 background image
extern const Picture ball; // A 19x19 purple ball with white boundaries
extern const Picture paddle; // A 59x5 paddle

const int border = 20;
int xmin; // Farthest to the left the center of the ball can go
int xmax; // Farthest to the right the center of the ball can go
int ymin; // Farthest to the top the center of the ball can go
int ymax; // Farthest to the bottom the center of the ball can go
int x,y; // Center of ball
int vx,vy; // Velocity components of ball

int px; // Center of paddle offset
int newpx; // New center of paddle

// This C macro will create an array of Picture elements.
// Really, you'll just use it as a pointer to a single Picture
// element with an internal pix2[] array large enough to hold
// an image of the specified size.
// BE CAREFUL HOW LARGE OF A PICTURE YOU TRY TO CREATE:
// A 100x100 picture uses 20000 bytes.  You have 32768 bytes of SRAM.
#define TempPicturePtr(name,width,height) Picture name[(width)*(height)/6+2] = { {width,height,2} }

// Create a 29x29 object to hold the ball plus padding
TempPicturePtr(object,29,29);

void TIM17_IRQHandler(void)
{
    TIM17->SR &= ~TIM_SR_UIF;
    //show_digit(scoreOff++);
    char key = check_key();
    globalKey = key;
    if (key == '*')
        newpx -= 1;
    else if (key == 'D')
        newpx += 1;
    if (newpx - paddle.width/2 <= border || newpx + paddle.width/2 >= 240-border)
        newpx = px;
    if (newpx != px) {
        px = newpx;
        // Create a temporary object two pixels wider than the paddle.
        // Copy the background into it.
        // Overlay the paddle image into the center.
        // Draw the result.
        //
        // As long as the paddle moves no more than one pixel to the left or right
        // it will clear the previous parts of the paddle from the screen.
        TempPicturePtr(tmp,61,5);
        pic_subset(tmp, &background, px-tmp->width/2, background.height-border-tmp->height); // Copy the background
        pic_overlay(tmp, 1, 0, &paddle, -1);
        LCD_DrawPicture(px-tmp->width/2, background.height-border-tmp->height, tmp);
    }
    x += vx;
    y += vy;
    if (x <= xmin) {
        // Ball hit the left wall.
    	setrgb(2);
        vx = - vx;
        if (x < xmin)
            x += vx;
        perturb(&vx,&vy);
    }
    if (x >= xmax) {
        // Ball hit the right wall.
    	setrgb(2);
        vx = -vx;
        if (x > xmax)
            x += vx;
        perturb(&vx,&vy);
    }
    if (y <= ymin) {
        // Ball hit the top wall.
    	setrgb(2);
        vy = - vy;
        if (y < ymin)
            y += vy;
        perturb(&vx,&vy);
    }
    if (y >= ymax - paddle.height &&
        x >= (px - paddle.width/2) &&
        x <= (px + paddle.width/2)) {
        // The ball has hit the paddle.  Bounce.
    	score += 10;
    	char str[4];
    	if(restartVal == 2){spi_display1("Playing again");}
    	sprintf(str, "Score: %d        ", score);
    	spi_display2(str);
    	setrgb(1);
        int pmax = ymax - paddle.height;
        vy = -vy;
        if (y > pmax)
            y += vy;
    }
    else if (y >= ymax) {
        // The ball has hit the bottom wall.  Set velocity of ball to 0,0.
    	setrgb(3);
        vx = 0;
        vy = 0;
        restartVal = 1;
        if(highScore < score)
        	highScore = score;
        restart();
    }

    TempPicturePtr(tmp,29,29); // Create a temporary 29x29 image.
    pic_subset(tmp, &background, x-tmp->width/2, y-tmp->height/2); // Copy the background
    pic_overlay(tmp, 0,0, object, 0xffff); // Overlay the object
    pic_overlay(tmp, (px-paddle.width/2) - (x-tmp->width/2),
            (background.height-border-paddle.height) - (y-tmp->height/2),
            &paddle, 0xffff); // Draw the paddle into the image
    LCD_DrawPicture(x-tmp->width/2,y-tmp->height/2, tmp); // Re-draw it to the screen
    // The object has a 5-pixel border around it that holds the background
    // image.  As long as the object does not move more than 5 pixels (in any
    // direction) from it's previous location, it will clear the old object.
}


void setup_usart5(){
	//Enable the RCC clocks to GPIOC and GPIOD.
	RCC->AHBENR |= RCC_AHBENR_GPIOCEN;
	RCC->AHBENR |= RCC_AHBENR_GPIODEN;
	//configure pin PC12 to be routed to USART5_TX.
	GPIOC->MODER &= ~0x3000000;
	GPIOC->MODER |= 0x2000000;
	GPIOC->AFR[1] |= 0x20000; //Check smaller manual to find alternate functions...
							  //ARF[0] is for low pins 0-7 and AFR[1] is for high pins 8-15. Use FRM to see which bit to set.
	//configure pin PD2 to be routed to USART5_RX.
	GPIOD->MODER &= ~0x30;
	GPIOD->MODER |= 0x20;
	GPIOD->AFR[0] |= 0x200;
	//Enable the RCC clock to the USART5 peripheral.
	RCC->APB1ENR |= RCC_APB1ENR_USART5EN;
	//Configure USART5 as follows:
	/*(First, disable it by turning off its UE bit.)
	Set a word size of 8 bits.
	Set it for one stop bit.
	Set it for no parity.
	Use 16x oversampling.
	Use a baud rate of 115200 (115.2 kbaud).
	Enable the transmitter and the receiver by settin gthe TE and RE bits..
	Enable the USART.
	Finally, you should wait for the TE and RE bits to be acknowledged.*/
	USART5->CR1 &= ~USART_CR1_UE;
	USART5->CR1 &= ~USART_CR1_M;
	USART5->CR2 &= ~USART_CR2_STOP;
	USART5->CR1 &= ~USART_CR1_PCE;
	USART5->CR1 &= ~USART_CR1_OVER8;
	USART5->BRR = 0x1A1;
	USART5->CR1 |= (USART_CR1_TE | USART_CR1_RE);
	USART5->CR1 |= USART_CR1_UE;
	while ((USART5->ISR & USART_ISR_TEACK) == 0);
	while ((USART5->ISR & USART_ISR_REACK) == 0);
}

int simple_putchar(int input){
	/*Wait for the USART5 ISR TXE to be set.
	Write the argument to the USART5 TDR (transmit data register).
	Return the argument that was passed in. That's how putchar() is defined to work.*/
	while ((USART5->ISR & USART_ISR_TXE) == 0);
	USART5->TDR = input;
	return input;
}

int simple_getchar(){
	while((USART5->ISR & USART_ISR_RXNE) == 0);
	return USART5->RDR;
}

int better_putchar(int input){
	if(input == '\n'){
		while ((USART5->ISR & USART_ISR_TXE) == 0);
		USART5->TDR = '\r';
		while ((USART5->ISR & USART_ISR_TXE) == 0);
		USART5->TDR = '\n';
	}
	else{
		while ((USART5->ISR & USART_ISR_TXE) == 0);
		USART5->TDR = input;
	}
	return input;
}

int better_getchar(){
	//examines the value read from the USART5 RDR. If it is a '\r' (carriage return), it returns a '\n' newline instead.
	while((USART5->ISR & USART_ISR_RXNE) == 0);
	int rvalue = USART5->RDR;
	if(rvalue == '\r')
		return '\n';
	else
		return rvalue;
}
// Write your subroutines above.

int __io_putchar(int ch) {
    return better_putchar(ch);
}

int __io_getchar(void) {
    return interrupt_getchar();
}

int interrupt_getchar(){
	for(;;){
		if(fifo_newline(&input_fifo) == 1)
			return fifo_remove(&input_fifo);
		else
			asm volatile ("wfi");
	}
}

void USART3_4_5_6_7_8_IRQHandler(){
	if (USART5->ISR & USART_ISR_ORE)
		USART5->ICR |= USART_ICR_ORECF;
	int read = USART5->RDR;
	if(fifo_full(&input_fifo))
		return;
	insert_echo_char(read);
}

void enable_tty_interrupt(){
	USART5->CR1 |= USART_CR1_RXNEIE;
	NVIC->ISER[0] = (1 << 29);
}

void spi_high_speed(){
	SPI1->CR1 &= ~SPI_CR1_SPE;

	SPI1->CR1 &= ~(SPI_CR1_BR);
	SPI1->CR1 |= SPI_CR1_BR_1;

	SPI1->CR1 |= SPI_CR1_SPE;
}

void restart(){
    // Draw the background.
	char string[100];
	spi_display1("Hold * to restart");
	for(int i = 0; i < 3500; i++){
		small_delay();
		if(restartVal == 1){
			globalKey = check_key();
		}
	}

    if (globalKey == 'D' && restartVal){
    	restartVal = 2;
		LCD_DrawPicture(0,0,&background);

		// Set all pixels in the object to white.
		for(int i=0; i<29*29; i++)
			object->pix2[i] = 0xffff;

		pic_overlay(object,5,5,&ball,0xffff);

		// Initialize the game state.
		xmin = border + ball.width/2;
		xmax = background.width - border - ball.width/2;
		ymin = border + ball.width/2;
		ymax = background.height - border - ball.height/2;
		x = (xmin+xmax)/2; // Center of ball
		y = ymin;
		vx = 0; // Velocity components of ball
		vy = 1;

		px = -1; // Center of paddle offset (invalid initial value to force update)
		newpx = (xmax+xmin)/2; // New center of paddle
		setbuf(stdin,0);
		setbuf(stdout,0);
		setbuf(stderr,0);
		score = 0;
		spi_display1("Restarting               ");
		spi_display2("Resetting Score");
	}
	else{
		goodbye7Seg();
		spi_display1("Goodbye!               ");
		sprintf(string, "High score: %d   ", highScore);
		spi_display2(string);
		TIM17->DIER &= ~TIM_DIER_UIE;
	}
}
int main(void)
{
	init_wavetable();
    setup_portb();
    setup_spi1();
    LCD_Init();

    // Draw the background.
    LCD_DrawPicture(0,0,&background);

    // Set all pixels in the object to white.
    for(int i=0; i<29*29; i++)
        object->pix2[i] = 0xffff;

    // Center the 19x19 ball into center of the 29x29 object.
    // Now, the 19x19 ball has 5-pixel white borders in all directions.
    pic_overlay(object,5,5,&ball,0xffff);

    // Initialize the game state.
    xmin = border + ball.width/2;
    xmax = background.width - border - ball.width/2;
    ymin = border + ball.width/2;
    ymax = background.height - border - ball.height/2;
    x = (xmin+xmax)/2; // Center of ball
    y = ymin;
    vx = 0; // Velocity components of ball
    vy = 1;

    px = -1; // Center of paddle offset (invalid initial value to force update)
    newpx = (xmax+xmin)/2; // New center of paddle
    setbuf(stdin,0);
    setbuf(stdout,0);
    setbuf(stderr,0);
    setup_usart5();
    enable_tty_interrupt();
    enable_ports();
    printf("Do you want to start a new game? (Y/N)\n");
    char line1 = 0;
    char line[100];
    char name[20];
    name[0] = '4';
    while(1){
    	line1 = fgetc(stdin);
    	if(line1 == 'y' || line1 == 'Y') {break;}
    	if(line1 == 'n' || line1 == 'N') {break;}
    	printf("Please answer Y/N");
    }
    int songNum = 1;
	if(line1 == 'Y' || line1 == 'y') {
		printf("Which song do you want to play? (1, 2, 3)\n");
		while(1){
			line1 = fgetc(stdin);
			if(line1 >= '1' && line1 <= '3') {
				songNum = line1 - '0';
				break;
			}
		}
		printf("Which difficulty do you want? (1, 2, 3)\n");
		while(1){
			line1 = fgetc(stdin);
			if(line1 >= '1' && line1 <= '3') {
				difficulty = line1 - '0';
				break;
			}
		}
		strcpy(line, "Hello Player1");
		setup_spi2();
		spi_init_oled();
		spi_display1(line);
		spi_display2("Score: ");
		setup_tim17(difficulty);
		setup_tim1();
		setup_tim6();
		setup_tim7(1);
	    updateScore();
		setup_tim2();
	}
	else{
		printf("See you next time!");
		nano_wait(1000000000);
		return 1;
	}

}
