#include <msp430.h>
#include <stdint.h>
#include <stdio.h>

#include <wisp-base.h>

#define PORT_LED_DIR P1DIR
#define PORT_LED_OUT P1OUT
#define PIN_LED      1

// For wisp-base
uint8_t usrBank[USRBANK_SIZE];

int main()
{
    uint32_t delay;

    WISP_init();
    UART_init();

    __enable_interrupt();

    PORT_LED_DIR |= PIN_LED;
    PORT_LED_OUT |= PIN_LED;

    while (1) {
        printf("Hello from MSP430\r\n");
        
        delay = 0xfffff;
        while (delay--);
    }

    return 0;
}
