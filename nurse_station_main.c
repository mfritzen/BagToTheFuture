/**
  ******************************************************************************
  * @file    main.c
  * @author  Ac6
  * @version V1.0
  * @date    01-December-2013
  * @brief   Default main function.
  ******************************************************************************
*/

#include "stm32f0xx.h"
#include "stm32f0_discovery.h"
#include <math.h>

int16_t wavetable[256];
int step;
int offset = 0;
int size = sizeof wavetable / sizeof wavetable[0];

int language = 0;

void dma_setup5();

uint16_t dispmem[34] = {
        0x080 + 0,
        0x220, 0x220, 0x220, 0x220, 0x220, 0x220, 0x220, 0x220,
        0x220, 0x220, 0x220, 0x220, 0x220, 0x220, 0x220, 0x220,
        0x080 + 64,
        0x220, 0x220, 0x220, 0x220, 0x220, 0x220, 0x220, 0x220,
        0x220, 0x220, 0x220, 0x220, 0x220, 0x220, 0x220, 0x220,
};

void nano_wait(unsigned int n)
{
    asm(    "        mov r0,%0\n"
            "repeat: sub r0,#83\n"
            "        bgt repeat\n" : : "r"(n) : "r0", "cc");
}

// *************** Code for Screen *************** //

void cmd(char b)
{
    while((SPI2 -> SR & SPI_SR_TXE) == 0)
        ; // wait for the transmit buffer to be empty

    // Deposit the data in the argument into the SPI channel 2 data register
    SPI2 -> DR = b;
}

void data(char b)
{
    while((SPI2 -> SR & SPI_SR_TXE) == 0)
        ; // wait for the transmit buffer to be empty

    // Deposit the data in the argument into the SPI channel 2 data register + 0x200
    SPI2 -> DR = 0x200 + b;
}

void display1(const char *s)
{
    // Place the cursor on the beginning of the first line (offset 0).
    cmd(0x80 + 0);
    int x;
    for(x=0; x<16; x+=1)
    {
        if (s[x])
        {
            int my_val = s[x];
            my_val |= 0x200;
            dispmem[x+1] = my_val;
        }

        else
            break;
    }

    for(   ; x<16; x+=1)
    {
        dispmem[x+1] = 0x220;
    }

    dma_setup5();
}

void display2(const char *s)
{
    // Place the cursor on the beginning of the second line (offset 64).
    cmd(0x80 + 64);
    int x;
    for(x=0; x<16; x+=1)
    {
        if (s[x])
        {
            int my_val = s[x];
            my_val |= 0x200;
            dispmem[x+18] = my_val;
        }

        else
            break;
    }

    for(   ; x<16; x+=1)
    {
        dispmem[x+18] = 0x220;
    }

    dma_setup5();
}

// *************** Setup for Screen *************** //

        // PB12 - NSS
        // PB13 - SCK
        // PB14 - MISO
        // PB15 - MOSI
void init_spi()
{
    RCC->AHBENR   |=  0x00040000;  // Enable clock to Port B
    GPIOB->MODER  &= ~0xcf000000;  // Clear the bits for AF
    GPIOB->MODER  |=  0x8a000000;  // Set the bits to '10' for each pin (alternate function)
    GPIOB->AFR[1] &= ~0xf0ff0000;  // Set the pins to use AF0
    RCC->APB1ENR  |=  0x00004000;  // Enable clock to SPI Channel 2

    SPI2->CR1 |=  0xc03c;  // bidirectional mode, bidirectional output enabled, master mode, lowest baud rate
    SPI2->CR1 &= ~0x0003;  // clock is 0 when idle, 1st clock transition is data capture edge
    SPI2->CR2 |=  0x0f00;  // clear data size
    SPI2->CR2 &= ~0x0600;  // 10-bit word size
    SPI2->CR2 |=  0x000c;  // Configure slave select output enable, and automatic NSS pulse generation.

    SPI2->CR1 |=  0x0040;  // Enable the SPI channel 2
}

void generic_lcd_startup(void)
{
    nano_wait(100000000); // Give it 100ms to initialize
    cmd(0x38);            // 0011 NF00 N=1, F=0: two lines
    cmd(0x0c);            // 0000 1DCB: display on, no cursor, no blink
    cmd(0x01);            // clear entire display
    nano_wait(6200000);   // clear takes 6.2ms to complete
    cmd(0x02);            // put the cursor in the home position
    cmd(0x06);            // 0000 01IS: set display to increment
}

void dma_setup5()
{
    RCC->AHBENR |= 0x1; // DMA clock enable

    DMA1_Channel5->CCR &= ~0x1; // ensure DMA is turned off
    DMA1_Channel5->CMAR = (uint32_t) dispmem; // copy from dispmem
    DMA1_Channel5->CPAR = (uint32_t) &(SPI2 -> DR); // copy to SPI2_DR
    DMA1_Channel5->CNDTR = 34;

    DMA1_Channel5->CCR &= ~0x0c00;  // clear MSIZE bits
    DMA1_Channel5->CCR |=  0x0400;  // MSIZE 01: 16-bits
    DMA1_Channel5->CCR &= ~0x0300;  // clear PSIZE bits
    DMA1_Channel5->CCR |=  0x0100;  // PSIZE 01: 16-bits
    DMA1_Channel5->CCR |=  0x0010;  // read from memory (DIR)
    DMA1_Channel5->CCR |=  0x0080;  // enable MINC
    DMA1_Channel5->CCR &= ~0x0040;  // disable PINC
    DMA1_Channel5->CCR &= ~0x3000;  // channel low priority (PL)
    DMA1_Channel5->CCR &= ~0x0020;  // enable circular mode (CIRC)
    DMA1_Channel5->CCR &= ~0x0002;  // disable TCI
    DMA1_Channel5->CCR &= ~0x0004;  // disable HTI
    DMA1_Channel5->CCR &= ~0x4000;  // disable MEM2MEM

    // modify SPI channel 2 so that a DMA request is made whenever the transmitter buffer is empty
    SPI2->CR2 |= 0x2; // (TXDMAEN)

    DMA1_Channel5->CCR |= 0x1;  // enable DMA channel 5
}

void init_lcd()
{
    display1("INITIALIZING");
    nano_wait(1000000000);
}

// *************** Code for Sound *************** //

void setup_timer2(int freq)
{
    RCC->APB1ENR |= 0x1;  // Enable the system clock for timer 2

    TIM2->CR1  &= ~0x0070;  // Set timer so that it is non-centered and up-counting
    TIM2->PSC   = 2 - 1;    // Set the Prescaler output to 24MHz (48MHz/2)
    TIM2->ARR   = (((24000000) / freq) / size) - 1;    // Auto-reload at 213 so that it reloads at 112676 Hz
    TIM2->DIER |=  0x0001;  // Enable timer 2 interrupt (set UIE bit)
    TIM2->CR1  |=  0x0001;  // Start the timer

    NVIC->ISER[0] = 1 << TIM2_IRQn;  // Enable timer 2 interrupt
}

void TIM2_IRQHandler()
{
    TIM2->SR     &= ~0x1;  // Acknowledge the interrupt
    DAC->DHR12R1  = wavetable[offset];
    DAC->SWTRIGR |=  0x1;  // Software trigger the DAC

    offset += step;

    if (offset >= size)
    {
        offset -= size;
    }
}

void setup_DAC()
{

    RCC->APB1ENR |=  0x20000000;  // Enable clock to DAC
    DAC->CR      &= ~0x00000001;  // Disable DAC first.
    RCC->AHBENR  |=  0x00020000;  // Enable clock to GPIOA
    GPIOA->MODER |=  0x00000300;  // Set PA4 for Analog mode
    DAC->CR      |=  0x00000038;  // Enable software trigger
    DAC->CR      |=  0x00000004;  // Enable trigger
    DAC->CR      |=  0x00000001;  // Enable DAC Channel 1
}

void init_sound()
{
    setup_DAC();
    // Initialize the sine wave
    for(int i = 0; i < size; i++)
    {
        wavetable[i] = (32767 * sin(i * 2 * M_PI / size) + 32768) / 16;
    }

    step = 1;
    int count;

    for (count = 0; count < 2; count++)
    {
        setup_timer2(440);
        nano_wait(50000000);
        TIM2->CR1 &= ~0x1;
        nano_wait(50000000);
    }
}

// *************** Setup Button *************** //

void setup_button()
{
    RCC->AHBENR |= 0x00000020;  // enable clock to port A
}

// *************** Setup Light **************** //

void setup_light()
{
    RCC->AHBENR  |= 0x00040000;  // enable clock to port B
    GPIOB->MODER |= 0x00400000;  // set pin 11 to output mode
}

// *************** Code for Testing *************** //

void test_lcd()
{
    for (int i = 0; i < 20; i++)
    {
        display1("THE SCREEN WORKS");
        display2("TEAM 11111111111");
        nano_wait(100000000);
    }
}

void test_sound()
{
    setup_DAC();
    for(int i = 0; i < size; i++)
    {
        wavetable[i] = (32767 * sin(i * 2 * M_PI / size) + 32768) / 16;
    }

    step = 1;
    int count;

    for (count = 0; count < 10; count++)
    {
        setup_timer2(440);
        nano_wait(100000000);
        TIM2->CR1 &= ~0x1;
    }
}

// *************** Standard Operation *************** //

void switch_lang()
{
    if (language == 0)
    {
        display1("ROOM LEVEL RATE");
        nano_wait(100000000);
    }
    if (language == 1)
    {
        display1("SALA NIVEL GOTEO");     // **** CHECK TRANSLATION **** //
        nano_wait(100000000);
    }
}

void instruct()
{
    int i;
    int len = 15;

    for (i = 0; i < len; i++)
    {
        display1("SCREEN SUPPORTS ");
        display2("ENGLISH/SPANISH ");
        nano_wait(100000000);
    }

    for (i = 0; i < len; i++)
    {
        display1("CLICK BUTTON TO ");
        display2("SCROLL ROOMS    ");
        nano_wait(100000000);
    }

    for (i = 0; i < len; i++)
    {
        display1("ALARM SOUNDS    ");
        display2("WHEN LEVEL < 10%");
        nano_wait(100000000);
    }

/*    for (i = 0; i < len; i++)
    {
        display1("USE SWITCH TO    ");
        display2("TOGGLE LANGUAGE  ");
        nano_wait(100000000);
    }*/

    // **** GET A BETTER TRANSLATION **** //

    for (i = 0; i < len; i++)
    {
        display1("APARATO RESPALDA");   // also bad
        display2("INGLES / ESPANOL");
        nano_wait(100000000);
    }

    for (i = 0; i < len; i++)
    {
        display1("PRESIONAR PULSADOR");  // this translation is bad
        display2("PARA VER LAS SALAS");
        nano_wait(100000000);
    }

    for (i = 0; i < len; i++)
        {
            display1("ALARMA CUANDO");
            display2("NIVEL < 10%");
            nano_wait(100000000);
        }

/*    for (i = 0; i < len; i++)
    {
        display1("USAR INTERRUPTOR");   // also bad
        display2("PARA CAMBIAR IDIOMA");
        nano_wait(100000000);
    }*/
}

// normal operation
void norm_op()
{
    int rm_cnt = 1;

    int rm_1_num = 101;
    int rm_1_lvl = 20;
    int rm_1_rt = 23;

    int rm_2_num = 102;
    int rm_2_lvl = 100;
    int rm_2_rt = 20;

    int rm_3_num = 103;
    int rm_3_lvl = 70;
    int rm_3_rt = 40;

    char rm_1_str[16];
    char rm_2_str[16];
    char rm_3_str[16];

    display1("ROOM LEVEL RATE");

    while(1)
    {
        sprintf(rm_1_str, "%d   %d   %d", rm_1_num, rm_1_lvl, rm_1_rt);
        sprintf(rm_2_str, "%d   %d   %d", rm_2_num, rm_2_lvl, rm_2_rt);
        sprintf(rm_3_str, "%d   %d   %d", rm_3_num, rm_3_lvl, rm_3_rt);

        // simulate input
        if ((GPIOA->IDR) & 0x2)
        {
            rm_1_lvl = 4;
        }
        else if ((GPIOA->IDR) & 0x4)
        {
            rm_1_lvl = 9;
        }
        else
        {
            rm_1_lvl = 20;
        }

        // when pushbutton is pressed
        if ((GPIOA->IDR) & 0x1)
        {
            rm_cnt++;
            nano_wait(100000000);
        }
        if (rm_cnt > 4)
        {
            rm_cnt = 1;
            nano_wait(100000000);
        }

        // toggle through rooms
        if (rm_cnt == 1)
        {
            display2(rm_1_str);
            nano_wait(100000000);
        }
        if (rm_cnt == 2)
        {
            display2(rm_2_str);
            nano_wait(100000000);
        }
        if (rm_cnt == 3)
        {
            display2(rm_3_str);
            nano_wait(100000000);
        }
        if (rm_cnt == 4)
        {
            display2("104   N/C   N/C ");
            nano_wait(100000000);
        }

        // emergency condition
        if ((rm_1_lvl <= 5) || (rm_2_lvl <= 5) || (rm_3_lvl <= 5))
        {
            GPIOB->ODR |= 0x0800;   // turn on LED
            setup_timer2(440);
            nano_wait(50000000);
            TIM2->CR1 &= ~0x1;
        }
        // refill condition
        else if ((rm_1_lvl <= 10) || (rm_2_lvl <= 10) || (rm_3_lvl <= 10))
        {
            GPIOB->ODR |= 0x0800;   // turn on LED
            setup_timer2(440);
            nano_wait(100000000);
            TIM2->CR1 &= ~0x1;
        }
        else
        {
            GPIOB->ODR &= ~0x0800;  // turn off LED
        }

        // when switch is toggled to change language
        if ((GPIOA->IDR) & 0x8)
        {
            if (language == 0)
            {
                language = 1;
                switch_lang();
            }
        else if (~((GPIOA->IDR) & 0x8))
            if (language == 1)
            {
                language = 0;
                switch_lang();
            }
        }
    }
}

int main()
{
    init_spi();
    generic_lcd_startup();
    init_lcd();
    setup_DAC();
    setup_button();
    setup_light();
    init_sound();

    instruct();
    norm_op();

    return(0);
}


