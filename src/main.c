#include <msp430.h>
#undef N // conflicts with us

#include <stdint.h>
#include <stdbool.h>
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

void mult(bigint_t out_block, bigint_t a, bigint_t b)
{
    int i;
    unsigned digit;
    digit_t p, c, dp;
    digit_t carry = 0;

    for (digit = 0; digit < NUM_DIGITS * 2; ++digit) {
        printf("mult: d=%u\r\n", digit);

        p = carry;
        c = 0;
        for (i = 0; i < NUM_DIGITS; ++i) {
            if (digit - i >= 0 && digit - i < NUM_DIGITS) {
                dp = a[digit - i] * b[i];

                c += dp >> DIGIT_BITS;
                p += dp & DIGIT_MASK;

                printf("mult: i=%u a=%x b=%x p=%x\r\n", i, a[digit - i], b[i], p);
            }
        }

        c += p >> DIGIT_BITS;
        p &= DIGIT_MASK;

        out_block[digit] = p;
        carry = c;
    }
}

bool reduce_normalizable(bigint_t m, const bigint_t n, unsigned d)
{
    int i;
    unsigned offset;
    digit_t n_d, m_d;
    bool normalizable = true;

    offset = d + 1 - NUM_DIGITS; // TODO: can this go below zero
    printf("reduce: normalizable: d=%u offset=%u\r\n", d, offset);

    for (i = d; i >= 0; --i) {
        m_d = m[i];
        n_d = n[i - offset];

        printf("normalizable: m[%u]=%x n[%u]=%x\r\n", i, m_d, i - offset, n_d);

        if (m_d > n_d) {
            break;
        } else if (m_d < n_d) {
            normalizable = false;
            break;
        }
    }

    return normalizable;
}

void reduce_normalize(bigint_t m, const bigint_t n, unsigned digit)
{
    int i;
    digit_t d, s, m_d, n_d;
    unsigned borrow, offset;

    offset = digit + 1 - NUM_DIGITS; // TODO: can this go below zero

    borrow = 0;
    for (i = 0; i < NUM_DIGITS; ++i) {
        m_d = m[i + offset];
        n_d = n[i];

        s = n_d + borrow;
        if (m_d < s) {
            m_d += 1 << DIGIT_BITS;
            borrow = 1;
        } else {
            borrow = 0;
        }
        d = m_d - s;

        printf("normalize: m[%u]=%x n[%u]=%x b=%u d=%x\r\n",
                i + offset, m_d, i, n_d, borrow, d);

        m[i + offset] = d;
    }
}

void reduce_quotient(digit_t *quotient, bigint_t m, const bigint_t n, unsigned d)
{
    digit_t m_d[3]; // [2]=m[d], [1]=m[d-1], [0]=m[d-2]
    digit_t q, n_div, n_n;
    uint32_t n_q, qn;

    // Divisor, derived from modulus, for refining quotient guess into exact value
    n_div = ((n[NUM_DIGITS - 1] << DIGIT_BITS) + n[NUM_DIGITS - 2]);

    n_n = n[NUM_DIGITS - 1];

    m_d[2] = m[d];
    m_d[1] = m[d - 1];
    m_d[0] = m[d - 2];

    printf("reduce: quotient: n_n=%x m[d]=%x\r\n", n_n, m[2]);

    // Choose an initial guess for quotient
    if (m_d[2] == n_n) {
        q = (1 << DIGIT_BITS) - 1;
    } else {
        q = ((m_d[2] << DIGIT_BITS) + m_d[1]) / n_n;
    }

    // Refine quotient guess

    // NOTE: An alternative to composing the digits into one variable, is to
    // have a loop that does the comparison digit by digit to implement the
    // condition of the while loop below.
    n_q = ((uint32_t)m_d[2] << (2 * DIGIT_BITS)) + (m_d[1] << DIGIT_BITS) + m_d[0];

    printf("reduce: quotient: m[d]=%x m[d-1]=%x m[d-2]=%x n_q=%x%x\r\n",
           m_d[2], m_d[1], m_d[0],
           (uint16_t)((n_q >> 16) & 0xffff), (uint16_t)(n_q & 0xffff));

    printf("reduce: quotient: q0=%x\r\n", q);

    q++;
    do {
        q--;
        qn = (uint32_t)n_div * q;
        printf("reduce: quotient: q=%x qn=%x%x\r\n", q,
              (uint16_t)((qn >> 16) & 0xffff), (uint16_t)(qn & 0xffff));
    } while (qn > n_q);

    // This is still not the final quotient, it may be off by one,
    // which we determine and fix in the 'compare' and 'add' steps.
    printf("reduce: quotient: q=%x\r\n", q);

    *quotient = q;
}

void reduce_multiply(bigint_t product, digit_t q, const bigint_t n, unsigned d)
{
    int i;
    unsigned offset, c;
    digit_t p, nd;

    // TODO: factor out shift outside of this task
    // As part of this task, we also perform the left-shifting of the q*m
    // product by radix^(digit-NUM_DIGITS), where NUM_DIGITS is the number
    // of digits in the modulus. We implement this by fetching the digits
    // of number being reduced at that offset.
    offset = d - NUM_DIGITS;
    printf("reduce: multiply: offset=%u\r\n", offset);

    // Left-shift zeros
    for (i = 0; i < offset; ++i)
        product[i] = 0;

    c = 0;
    for (i = offset; i < 2 * NUM_DIGITS; ++i) {

        // This condition creates the left-shifted zeros.
        // TODO: consider adding number of digits to go along with the 'product' field,
        // then we would not have to zero out the MSDs
        p = c;
        if (i < offset + NUM_DIGITS) {
            nd = n[i - offset];
            p += q * nd;
        } else {
            nd = 0;
            // TODO: could break out of the loop  in this case (after CHAN_OUT)
        }

        printf("reduce: multiply: n[%u]=%x q=%x c=%x m[%u]=%x\r\n",
               i - offset, nd, q, c, i, p);

        c = p >> DIGIT_BITS;
        p &= DIGIT_MASK;

        product[i] = p;
    }
}

int reduce_compare(bigint_t a, bigint_t b)
{
    int i;
    int relation = 0;

    for (i = NUM_DIGITS * 2 - 1; i >= 0; --i) {
        if (a > b) {
            relation = 1;
            break;
        } else if (a < b) {
            relation = -1;
            break;
        }
    }

    return relation;
}

void reduce_add(bigint_t a, const bigint_t b, unsigned d)
{
    int i, j;
    unsigned offset, c;
    digit_t r, m, n;

    // TODO: factor out shift outside of this task
    // Part of this task is to shift modulus by radix^(digit - NUM_DIGITS)
    offset = d - NUM_DIGITS;

    c = 0;
    for (i = offset; i < 2 * NUM_DIGITS; ++i) {
        m = a[i];

        // Shifted index of the modulus digit
        j = i - offset;

        if (i < offset + NUM_DIGITS) {
            n = b[j];
        } else {
            n = 0;
            j = 0; // a bit ugly, we want 'nan', but ok, since for output only
            // TODO: could break out of the loop in this case (after CHAN_OUT)
        }

        r = c + m + n;

        printf("reduce: add: m[%u]=%x n[%u]=%x c=%x r=%x\r\n", i, m, j, n, c, r);

        c = r >> DIGIT_BITS;
        r &= DIGIT_MASK;

        a[i] = r;
    }
}

void reduce_subtract(bigint_t a, bigint_t b, unsigned d)
{
    int i;
    digit_t m, s, r, qn;
    unsigned borrow, offset;

    // TODO: factor out shifting logic from this task
    // The qn product had been shifted by this offset, no need to subtract the zeros
    offset = d - NUM_DIGITS;

    printf("reduce: subtract: d=%u offset=%u\r\n", d, offset);

    borrow = 0;
    for (i = offset; i < 2 * NUM_DIGITS; ++i) {
        m = a[i];

        qn = b[i];

        s = qn + borrow;
        if (m < s) {
            m += 1 << DIGIT_BITS;
            borrow = 1;
        } else {
            borrow = 0;
        }
        r = m - s;

        printf("reduce: subtract: m[%u]=%x qn[%u]=%x b=%u r=%x\r\n",
               i, m, i, qn, borrow, r);

        a[i] = r;
    }
}

void reduce(bigint_t m, const bigint_t n)
{
    digit_t q, m_d;
    bigint_t qn;
    unsigned d;

    // Start reduction loop at most significant non-zero digit
    d = 2 * NUM_DIGITS;
    do {
        d--;
        m_d = m[d];
        printf("reduce digits: p[%u]=%x\r\n", d, m_d);
    } while (m_d == 0 && d > 0);

    if (reduce_normalizable(m, n, d)) {
        reduce_normalize(m, n, d);
    } else if (d == NUM_DIGITS - 1) {
        printf("reduce: done: message < modulus\r\n");
        return;
    }

    while (d > NUM_DIGITS) {
        reduce_quotient(&q, m, n, d);
        reduce_multiply(qn, q, n, d);
        if (reduce_compare(m, qn) < 0)
            reduce_add(m, n, d);
        reduce_subtract(m, qn, d);
        d--;
    }
}

void mod_mult(bigint_t out_block, bigint_t a, bigint_t b, const bigint_t n)
{
    mult(out_block, a, b);
    reduce(out_block, n);
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

    while (1) {
        blink(1, SEC_TO_CYCLES, LED1 | LED2);
    }

    return 0;
}
