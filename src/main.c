#include <msp430.h>
#undef N // conflicts with us

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

#include <libmsp/mem.h>
#include <wisp-base.h>
#include <msp-math.h>

#ifdef CONFIG_EDB
#include <libedb/edb.h>
#endif

#include "pins.h"

// #define VERBOSE
// #define SHOW_PROGRESS_ON_LED

#ifdef VERBOSE
#define LOG printf
#else
#define LOG(...)
#endif // VERBOSE

#define PORT_LED_DIR P1DIR
#define PORT_LED_OUT P1OUT
#define PIN_LED      1

#if defined(CONFIG_LIBEDB_PRINTF_EIF)
#define printf(...) PRINTF(__VA_ARGS__)
#elif defined(CONFIG_LIBEDB_PRINTF_BARE)
#define printf(...) BARE_PRINTF(__VA_ARGS__)
#elif defined(CONFIG_LIBMSPCONSOLE_PRINTF)
// nothing to to do
#else
#define printf(...)
#endif


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

#define PRINT_HEX_ASCII_COLS 8

/** @brief Type large enough to store a product of two digits */
typedef uint16_t digit_t;

/** @brief Multi-digit integer */
typedef unsigned bigint_t[NUM_DIGITS * 2];

typedef struct {
    bigint_t n; // modulus
    digit_t e;  // exponent
} pubkey_t;

// Blocks are padded with these digits (on the MSD side). Padding value must be
// chosen such that block value is less than the modulus. This is accomplished
// by any value below 0x80, because the modulus is restricted to be above
// 0x80 (see comments below).
static __ro_nv const uint8_t PAD_DIGITS[] = { 0x01 };
#define NUM_PAD_DIGITS (sizeof(PAD_DIGITS) / sizeof(PAD_DIGITS[0]))

// To generate a key pair:
// $ openssl genrsa -out private.pem -3 32
// $ openssl rsa -out keys.txt -text -in private.pem
// Note: genrsa is superceded by genpkey, but the latter supports only >256-bit

static __ro_nv const pubkey_t pubkey = {
    .n = { 0x45, 0x6a, 0x49, 0xaa }, // byte order: LSB to MSB, constraint MSB>=0x80
    .e = 0x3,
};
// private exponent for the above: 0x71853073

//static __ro_nv const unsigned char PLAINTEXT[] = "Hello, World!";
static __ro_nv const unsigned char PLAINTEXT[] = "Hello, World!";

#define CYPHERTEXT_BUF_SIZE 32
static __nv uint8_t CYPHERTEXT[CYPHERTEXT_BUF_SIZE] = {0};
static __nv unsigned CYPHERTEXT_LEN = 0;

#ifdef SHOW_PROGRESS_ON_LED
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
#endif

void print_bigint(const bigint_t n, unsigned digits)
{
    int i;
    for (i = digits - 1; i >= 0; --i)
        printf("%02x ", n[i]);
}

void log_bigint(const bigint_t n, unsigned digits)
{
    int i;
    for (i = digits - 1; i >= 0; --i)
        LOG("%02x ", n[i]);
}

void print_hex_ascii(const uint8_t *m, unsigned len)
{
    int i, j;
   
    for (i = 0; i < len; i += PRINT_HEX_ASCII_COLS) {
        for (j = 0; j < PRINT_HEX_ASCII_COLS && i + j < len; ++j)
            printf("%02x ", m[i + j]);
        for (; j < PRINT_HEX_ASCII_COLS; ++j)
            printf("   ");
        printf(" ");
        for (j = 0; j < PRINT_HEX_ASCII_COLS && i + j < len; ++j) {
            char c = m[i + j];
            if (!(32 <= c && c <= 127)) // not printable
                c = '.';
            printf("%c", c);
        }
        printf("\r\n");
    }
}

void mult(bigint_t a, bigint_t b)
{
    // Store in NV memory to reduce RAM footprint (might not even fit in RAM)
    static __nv bigint_t product;

    int i;
    unsigned digit;
    digit_t p, c, dp;
    digit_t carry = 0;

    LOG("mult: a = "); log_bigint(a, NUM_DIGITS); LOG("\r\n");
    LOG("mult: b = "); log_bigint(b, NUM_DIGITS); LOG("\r\n");

    for (digit = 0; digit < NUM_DIGITS * 2; ++digit) {
        LOG("mult: d=%u\r\n", digit);

        p = carry;
        c = 0;
        for (i = 0; i < NUM_DIGITS; ++i) {
            if (i <= digit && digit - i < NUM_DIGITS) {
                dp = a[digit - i] * b[i];

                c += dp >> DIGIT_BITS;
                p += dp & DIGIT_MASK;

                LOG("mult: i=%u a=%x b=%x dp=%x p=%x\r\n", i, a[digit - i], b[i], dp, p);
            }
        }

        c += p >> DIGIT_BITS;
        p &= DIGIT_MASK;

        product[digit] = p;
        carry = c;
    }

    LOG("mult: product = "); log_bigint(product, 2 * NUM_DIGITS); LOG("\r\n");

    for (i = 0; i < 2 * NUM_DIGITS; ++i)
        a[i] = product[i];
}

bool reduce_normalizable(bigint_t m, const bigint_t n, unsigned d)
{
    int i;
    unsigned offset;
    digit_t n_d, m_d;
    bool normalizable = true;

    offset = d + 1 - NUM_DIGITS; // TODO: can this go below zero
    LOG("reduce: normalizable: d=%u offset=%u\r\n", d, offset);

    for (i = d; i >= 0; --i) {
        m_d = m[i];
        n_d = n[i - offset];

        LOG("normalizable: m[%u]=%x n[%u]=%x\r\n", i, m_d, i - offset, n_d);

        if (m_d > n_d) {
            break;
        } else if (m_d < n_d) {
            normalizable = false;
            break;
        }
    }

    LOG("normalizable: %u\r\n", normalizable);

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

        LOG("normalize: m[%u]=%x n[%u]=%x b=%u d=%x\r\n",
                i + offset, m_d, i, n_d, borrow, d);

        m[i + offset] = d;
    }
}

void reduce_quotient(digit_t *quotient, bigint_t m, const bigint_t n, unsigned d)
{
    digit_t m_d[3]; // [2]=m[d], [1]=m[d-1], [0]=m[d-2]
    digit_t q, n_div, n_n;
    uint32_t n_q, qn;
    uint16_t m_dividend;

    // Divisor, derived from modulus, for refining quotient guess into exact value
    n_div = ((n[NUM_DIGITS - 1] << DIGIT_BITS) + n[NUM_DIGITS - 2]);

    n_n = n[NUM_DIGITS - 1];

    m_d[2] = m[d];
    m_d[1] = m[d - 1];
    m_d[0] = m[d - 2];

    LOG("reduce: quotient: n_n=%x m[d]=%x\r\n", n_n, m[2]);

    // Choose an initial guess for quotient
    if (m_d[2] == n_n) {
        q = (1 << DIGIT_BITS) - 1;
    } else {
        // TODO: The long todo described below applies here.
        m_dividend = (m_d[2] << DIGIT_BITS) + m_d[1];
        q = m_dividend / n_n;
        LOG("reduce quotient: m_dividend=%x q=%x\r\n", m_dividend, q);
    }

    // Refine quotient guess

    // TODO: An alternative to composing the digits into one variable, is to
    // have a loop that does the comparison digit by digit to implement the
    // condition of the while loop below.
    n_q = ((uint32_t)m_d[2] << (2 * DIGIT_BITS)) + (m_d[1] << DIGIT_BITS) + m_d[0];

    LOG("reduce: quotient: m[d]=%x m[d-1]=%x m[d-2]=%x n_q=%02x%02x\r\n",
           m_d[2], m_d[1], m_d[0],
           (uint16_t)((n_q >> 16) & 0xffff), (uint16_t)(n_q & 0xffff));

    LOG("reduce: quotient: q0=%x\r\n", q);

    q++;
    do {
        q--;
        // NOTE: yes, this result can be >16-bit because:
        //   n_n min = 0x80 (by constraint on modulus msb being set)
        //   => q max = 0xffff / 0x80 = 0x1ff
        //   n_div max = 0x80ff
        //   => qn max = 0x80ff * 0x1ff = 0x1017d01
        //
        // TODO:
        //
        // Approach (1): stick to 16-bit operations, and implement any wider
        // ops as part of this application, ie. a function operating on digits.
        //
        // Approach (2)*: implement the needed intrinsics in Clang's compiler-rt,
        // using libgcc's ones for inspiration (watching the calling convention
        // carefully).
        //
        qn = mult16(n_div, q);
        LOG("reduce: quotient: q=%x n_div=%x qn=%02x%02x\r\n", q, n_div,
              (uint16_t)((qn >> 16) & 0xffff), (uint16_t)(qn & 0xffff));
    } while (qn > n_q);

    // This is still not the final quotient, it may be off by one,
    // which we determine and fix in the 'compare' and 'add' steps.
    LOG("reduce: quotient: q=%x\r\n", q);

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
    LOG("reduce: multiply: offset=%u\r\n", offset);

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

        LOG("reduce: multiply: n[%u]=%x q=%x c=%x m[%u]=%x\r\n",
               i - offset, nd, q, c, i, p);

        c = p >> DIGIT_BITS;
        p &= DIGIT_MASK;

        product[i] = p;
    }

    LOG("reduce: multiply: product = "); log_bigint(product, 2 * NUM_DIGITS); LOG("\r\n");
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

        LOG("reduce: add: m[%u]=%x n[%u]=%x c=%x r=%x\r\n", i, m, j, n, c, r);

        c = r >> DIGIT_BITS;
        r &= DIGIT_MASK;

        a[i] = r;
    }

    LOG("reduce: add: sum = "); log_bigint(a, 2 * NUM_DIGITS); LOG("\r\n");
}

void reduce_subtract(bigint_t a, bigint_t b, unsigned d)
{
    int i;
    digit_t m, s, r, qn;
    unsigned borrow, offset;

    // TODO: factor out shifting logic from this task
    // The qn product had been shifted by this offset, no need to subtract the zeros
    offset = d - NUM_DIGITS;

    LOG("reduce: subtract: d=%u offset=%u\r\n", d, offset);

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

        LOG("reduce: subtract: m[%u]=%x qn[%u]=%x b=%u r=%x\r\n",
               i, m, i, qn, borrow, r);

        a[i] = r;
    }

    LOG("reduce: subtract: sum = "); log_bigint(a, 2 * NUM_DIGITS); LOG("\r\n");
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
        LOG("reduce digits: p[%u]=%x\r\n", d, m_d);
    } while (m_d == 0 && d > 0);

    LOG("reduce digits: d=%x\r\n", d);

    if (reduce_normalizable(m, n, d)) {
        reduce_normalize(m, n, d);
    } else if (d == NUM_DIGITS - 1) {
        LOG("reduce: done: message < modulus\r\n");
        return;
    }

    while (d >= NUM_DIGITS) {
        reduce_quotient(&q, m, n, d);
        reduce_multiply(qn, q, n, d);
        if (reduce_compare(m, qn) < 0)
            reduce_add(m, n, d);
        reduce_subtract(m, qn, d);
        d--;
    }

    LOG("reduce: num = "); log_bigint(m, NUM_DIGITS); LOG("\r\n");
}

void mod_mult(bigint_t a, bigint_t b, const bigint_t n)
{
    mult(a, b);
    reduce(a, n);
}

void mod_exp(bigint_t out_block, bigint_t base, digit_t e, const bigint_t n)
{
    int i;

    // Result initialized to 1
    out_block[0] = 0x1;
    for (i = 1; i < NUM_DIGITS; ++i)
        out_block[i] = 0x0;

    while (e > 0) {
        LOG("mod exp: e=%x\r\n", e);

        if (e & 0x1)
            mod_mult(out_block, base, n);
        mod_mult(base, base, n);
        e >>= 1;
    }
}

void encrypt(uint8_t *cyphertext, unsigned *cyphertext_len,
             const uint8_t *message, unsigned message_length,
             const pubkey_t *k)
{
    // Store in NV memory to reduce RAM footprint (might not even fit in RAM)
    static __nv bigint_t in_block;
    static __nv bigint_t out_block;

    int i;
    unsigned in_block_offset, out_block_offset;

    in_block_offset = 0;
    out_block_offset = 0;
    while (in_block_offset < message_length) {
        for (i = 0; i < NUM_DIGITS - NUM_PAD_DIGITS; ++i)
            in_block[i] = (in_block_offset + i < message_length) ?
                                message[in_block_offset + i] : 0xFF;
        for (i = 0; i < NUM_PAD_DIGITS; ++i)
            in_block[NUM_DIGITS - NUM_PAD_DIGITS + i] = PAD_DIGITS[i];

        LOG("in block: "); log_bigint(in_block, NUM_DIGITS); LOG("\r\n");
        mod_exp(out_block, in_block, k->e, k->n);
        LOG("out block: "); log_bigint(out_block, NUM_DIGITS); LOG("\r\n");

        for (i = 0; i < NUM_DIGITS; ++i)
            cyphertext[out_block_offset + i] = out_block[i];

        in_block_offset += NUM_DIGITS - NUM_PAD_DIGITS;
        out_block_offset += NUM_DIGITS;

#ifdef VERBOSE
        uint32_t delay = 0xfffff;
        while (delay--);
#endif
    }

    *cyphertext_len = out_block_offset;
}

void init()
{
    WISP_init();

#ifdef CONFIG_EDB
    debug_setup();
#endif

#if defined(CONFIG_LIBEDB_PRINTF_BARE)
    BARE_PRINTF_ENABLE();
#elif defined(CONFIG_LIBMSPCONSOLE_PRINTF)
    UART_init();
#endif

    __enable_interrupt();

    GPIO(PORT_LED_1, DIR) |= BIT(PIN_LED_1);
    GPIO(PORT_LED_2, DIR) |= BIT(PIN_LED_2);
#if defined(PORT_LED_3)
    GPIO(PORT_LED_3, DIR) |= BIT(PIN_LED_3);
#endif

#if defined(PORT_LED_3) // when available, this LED indicates power-on
    GPIO(PORT_LED_3, OUT) |= BIT(PIN_LED_3);
#endif

    printf("RSA app booted\r\n");
#ifdef SHOW_PROGRESS_ON_LED
    blink(1, SEC_TO_CYCLES * 5, LED1 | LED2);
#endif

}

int main()
{
    unsigned message_length;

#ifndef MEMENTOS
    init();
#endif

    while (1) {

        message_length = sizeof(PLAINTEXT);

        printf("Message:\r\n"); print_hex_ascii(PLAINTEXT, message_length);
        printf("Public key: N = "); print_bigint(pubkey.n, NUM_DIGITS);
        printf(" E = %x\r\n", pubkey.e);

#ifdef SHOW_PROGRESS_ON_LED
        GPIO(PORT_LED_1, OUT) |= BIT(PIN_LED_1);
#endif

        encrypt(CYPHERTEXT, &CYPHERTEXT_LEN, PLAINTEXT, message_length, &pubkey);

#ifdef SHOW_PROGRESS_ON_LED
        GPIO(PORT_LED_1, OUT) &= ~BIT(PIN_LED_1);
#endif

        printf("Cyphertext:\r\n"); print_hex_ascii(CYPHERTEXT, CYPHERTEXT_LEN);

#ifdef SHOW_PROGRESS_ON_LED
        blink(1, SEC_TO_CYCLES, LED2);
#endif
    }

    return 0;
}
