#include <msp430.h>
#undef N // conflicts with us

#include <stdint.h>
#include <stdio.h>

#include <wisp-base.h>

#include "pins.h"

#define PORT_LED_DIR P1DIR
#define PORT_LED_OUT P1OUT
#define PIN_LED      1

// For wisp-base
uint8_t usrBank[USRBANK_SIZE];

#define NUM_DIGITS       4 // 4 * 8 = 32-bit blocks
#define DIGIT_BITS       8 // arithmetic ops take 8-bit args produce 16-bit result
#define DIGIT_MASK       0x00ff

#if NUM_DIGITS < 2
#error The modular reduction implementation requires at least 2 digits
#endif

#define LED1 (1 << 0)
#define LED2 (1 << 1)

#define SEC_TO_CYCLES 4000000 /* 4 MHz */

/** @brief Type large enough to store a product of two digits */
typedef uint16_t digit_t;

/** @brief Multi-digit integer */
typedef unsigned bigint_t[NUM_DIGITS];

// Blocks are padded with these digits (on the MSD side). Padding value must be
// chosen such that block value is less than the modulus. This is accomplished
// by any value below 0x80, because the modulus is restricted to be above
// 0x80 (see comments below).
static const uint8_t PAD_DIGITS[] = { 0x01 };
#define NUM_PAD_DIGITS (sizeof(PAD_DIGITS) / sizeof(PAD_DIGITS[0]))

// Test input
static const uint8_t A[] = { 0x40, 0x30, 0x20, 0x10 };
static const uint8_t B[] = { 0xB0, 0xA0, 0x90, 0x80 };

static const bigint_t N = { 0x80, 0x49, 0x60, 0x01 }; // modulus (see note below)
static const digit_t E = 0x11; // encryption exponent

// padded message blocks (padding is the first byte (0x01), rest is payload)
static const uint8_t M[] = {
    0x55, 0x3D, 0xEF, 0xC0, 0x4A, 0x92,
};

static void delay(uint32_t cycles)
{
    unsigned i;
    for (i = 0; i < cycles / (1U << 15); ++i)
        __delay_cycles(1U << 15);
}

static void blink(unsigned count, uint32_t duration, unsigned leds)
{
    unsigned i;
    for (i = 0; i < count; ++i) {
        GPIO(PORT_LED_1, OUT) |= (leds & LED1) ? BIT(PIN_LED_1) : 0x0;
        GPIO(PORT_LED_2, OUT) |= (leds & LED2) ? BIT(PIN_LED_2) : 0x0;
        delay(duration / 2);
        GPIO(PORT_LED_1, OUT) &= (leds & LED1) ? ~BIT(PIN_LED_1) : ~0x0;
        GPIO(PORT_LED_2, OUT) &= (leds & LED2) ? ~BIT(PIN_LED_2) : ~0x0;
        delay(duration / 2);
    }
}

void print_bigint(const bigint_t n)
{
    int i;
    for (i = NUM_DIGITS - 1; i < NUM_DIGITS; --i)
        printf("%x ", n[i]);
    printf("\r\n");
}

void mod_mult(bigint_t out_block, bigint_t a, bigint_t b, const bigint_t n)
{
    // TODO
}

void mod_exp(bigint_t out_block, bigint_t base, digit_t e, const bigint_t n)
{
    while (e > 0) {
        printf("mod exp: e=%x\r\n", e);

        if (e & 0x1)
            mod_mult(out_block, out_block, base, n);
        mod_mult(base, base, base, n);
        e >>= 1;
    }
}

int main()
{
    uint32_t delay;
    bigint_t in_block, out_block;
    unsigned block_offset;
    unsigned message_length;
    int i;
    digit_t e = E;

    WISP_init();
    UART_init();

    __enable_interrupt();

    PORT_LED_DIR |= PIN_LED;
    PORT_LED_OUT |= PIN_LED;

    printf("RSA app booted\r\n");
    blink(1, SEC_TO_CYCLES * 2, LED1 | LED2);

    message_length = sizeof(M);
    block_offset = 0;
    while (block_offset < message_length) {
        for (i = 0; i < NUM_DIGITS - NUM_PAD_DIGITS; ++i)
            in_block[i] = (block_offset + i < message_length) ? M[block_offset + i] : 0x00;
        for (i = NUM_DIGITS - NUM_PAD_DIGITS; i < NUM_DIGITS; ++i)
            in_block[i] = PAD_DIGITS[i];

        printf("in block: "); print_bigint(in_block);
        mod_exp(out_block, in_block, e, N);
        printf("out block: "); print_bigint(out_block);

        delay = 0xfffff;
        while (delay--);
    }

    printf("message done\r\n");

    while (1);

    return 0;
}
