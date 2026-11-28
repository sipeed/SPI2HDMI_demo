/*
 * STC8G1K17A / STC8G1K08A (8-pin) board support, SDCC.
 *
 *   pin 7  P3.2  SCLK  -> SCK        pin 1  P5.4  MOSI -> IO0
 *   pin 8  P3.3  GPIO  -> CS         pin 3  P5.5  GPIO -> IO1 (D/C)
 *   pin 5  P3.0  RxD   <- adapter TXD (download + keys)
 *   pin 6  P3.1  TxD   -> adapter RXD
 *
 * Set the internal oscillator to FOSC in the download tool (default 24 MHz).
 * Sending the text "@STCISP#" over the serial port jumps into the built-in download program, so a new
 * image can be loaded without cycling the power (STC-ISP: "reset to ISP on user command").
 * Register addresses are taken from the STC8G manual.
 */
#include "hal.h"

#ifndef FOSC
#define FOSC 24000000UL
#endif
#define BAUD 115200UL

__sfr __at (0x88) TCON;   __sfr __at (0x89) TMOD;   __sfr __at (0x8A) TL0;   __sfr __at (0x8C) TH0;
__sfr __at (0x8B) TL1;    __sfr __at (0x8D) TH1;    __sfr __at (0x8E) AUXR;
__sfr __at (0x98) SCON;   __sfr __at (0x99) SBUF;
__sfr __at (0xB0) P3;     __sfr __at (0xB1) P3M1;   __sfr __at (0xB2) P3M0;
__sfr __at (0xC8) P5;     __sfr __at (0xC9) P5M1;   __sfr __at (0xCA) P5M0;
__sfr __at (0xC7) IAP_CONTR;
__sfr __at (0xCD) SPSTAT; __sfr __at (0xCE) SPCTL;  __sfr __at (0xCF) SPDAT;
__sbit __at (0x8D) TF0;   __sbit __at (0x8C) TR0;   __sbit __at (0x8E) TR1;
__sbit __at (0x98) RI;
__sbit __at (0xB3) CS;    /* P3.3 */
__sbit __at (0xCD) DC;    /* P5.5 */

#define T0_RELOAD (65536UL - FOSC / 12 / 60)          /* 1/60 s */
#define T1_RELOAD (65536UL - FOSC / 4 / BAUD)

static void spi(uint8_t b)
{
    SPDAT = b;
    while (!(SPSTAT & 0x80)) ;
    SPSTAT = 0xC0;
}

void hal_init(void)
{
    CS = 1; DC = 1;
    P3M1 = 0x00; P3M0 = 0x0C;            /* P3.2 P3.3 push-pull, P3.0 P3.1 quasi-bidirectional */
    P5M1 = 0x00; P5M0 = 0x30;            /* P5.4 P5.5 push-pull */
    SPSTAT = 0xC0;
    SPCTL = 0xD0;                        /* SSIG | SPEN | MSTR, mode 0, MSB first, SYSclk / 4 */

    AUXR = (AUXR & ~0x81) | 0x40;        /* T0 12T, T1 1T, UART1 baud from T1 */
    TMOD = 0x00;                         /* both timers 16-bit auto-reload */
    TH0 = T0_RELOAD >> 8; TL0 = (uint8_t)T0_RELOAD;
    TH1 = T1_RELOAD >> 8; TL1 = (uint8_t)T1_RELOAD;
    SCON = 0x50;                         /* 8N1, receive enabled */
    TR0 = 1; TR1 = 1;
}

void hal_cmd(uint8_t c)  { CS = 0; DC = 0; spi(c); DC = 1; }
void hal_data(uint8_t b) { spi(b); }
void hal_end(void)       { CS = 1; }

uint8_t hal_key(void)
{
    static const char __code magic[] = "@STCISP#";
    static uint8_t mi;
    uint8_t c;
    if (!RI) return 0;
    RI = 0;
    c = SBUF;
    if (c == magic[mi]) { if (!magic[++mi]) IAP_CONTR = 0x60; }     /* soft reset into the ISP area */
    else mi = (c == magic[0]);
    return c;
}

void hal_wait_tick(void)
{
    while (!TF0) ;
    TF0 = 0;
}

uint8_t hal_tick_poll(void)
{
    if (!TF0) return 0;
    TF0 = 0;
    return 1;
}
