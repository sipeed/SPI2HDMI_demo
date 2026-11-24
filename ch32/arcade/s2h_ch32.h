/*
 * s2h_ch32.h - SPI2HDMI module driver for CH32V00x (ch32fun framework).
 *
 * Wiring (CH32V003 default SPI1 mapping; adjust for other parts):
 *   PC5 = SCK  -> module SCK
 *   PC6 = MOSI -> module IO0 (MOSI)
 *   PC4 = CS   -> module CS
 *   PC3 = D/C  -> module IO1 (DC)
 *   GND        -> module GND (power the module from 5V)
 * SPI mode 0, MSB first, 24MHz (HCLK/2).
 */
#ifndef S2H_CH32_H
#define S2H_CH32_H

#include "ch32fun.h"

#define S2H_SWRESET  0x01
#define S2H_CASET    0x2A
#define S2H_PASET    0x2B
#define S2H_RAMWR    0x2C
#define S2H_COLMOD   0x3A
#define S2H_SCALESET 0x88
#define S2H_TINTSET  0x89
#define S2H_PALSET   0x8A
#define S2H_MODESET  0x8B

#define S2H_MODE_640X480   0
#define S2H_MODE_1280X720  1

/* VGA SCALESET codes: 1x/2x/4x/8x/16x. */
#define S2H_SCALE_1X   0
#define S2H_SCALE_2X   1
#define S2H_SCALE_4X   2
#define S2H_SCALE_8X   3
#define S2H_SCALE_16X  4

/* 720p SCALESET codes: 2x/4x/8x/16x; 720p has no 1x source mode. */
#define S2H_SCALE_720P_2X   0
#define S2H_SCALE_720P_4X   1
#define S2H_SCALE_720P_8X   2
#define S2H_SCALE_720P_16X  3

#define S2H_FMT_RGB565 0x55
#define S2H_FMT_RGB332 0x32
#define S2H_FMT_GRAY4  0x14
#define S2H_FMT_PAL16  0x24
#define S2H_FMT_GRAY2  0x12
#define S2H_FMT_MONO1  0x11

#define S2H_CS_LOW()  (GPIOC->BSHR = (1 << (16 + 4)))
#define S2H_CS_HIGH() (GPIOC->BSHR = (1 << 4))
#define S2H_DC_CMD()  (GPIOC->BSHR = (1 << (16 + 3)))
#define S2H_DC_DAT()  (GPIOC->BSHR = (1 << 3))

static void s2h_spi_wait_txe(void)  { while (!(SPI1->STATR & SPI_STATR_TXE)); }
static void s2h_spi_wait_idle(void) { while (SPI1->STATR & SPI_STATR_BSY); }

static void s2h_byte(uint8_t b)
{
    s2h_spi_wait_txe();
    SPI1->DATAR = b;
}

static void s2h_cmd(uint8_t c)
{
    S2H_CS_LOW();
    S2H_DC_CMD();
    s2h_byte(c);
    s2h_spi_wait_idle();
    S2H_DC_DAT();
}

static void s2h_end(void)
{
    s2h_spi_wait_idle();
    S2H_CS_HIGH();
}

static void s2h_cmd1(uint8_t c, uint8_t p)
{
    s2h_cmd(c);
    s2h_byte(p);
    s2h_end();
}

static void s2h_window(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1)
{
    s2h_cmd(S2H_CASET);
    s2h_byte(x0 >> 8); s2h_byte(x0 & 0xFF);
    s2h_byte(x1 >> 8); s2h_byte(x1 & 0xFF);
    s2h_end();
    s2h_cmd(S2H_PASET);
    s2h_byte(y0 >> 8); s2h_byte(y0 & 0xFF);
    s2h_byte(y1 >> 8); s2h_byte(y1 & 0xFF);
    s2h_end();
}

static void s2h_tint(uint8_t r, uint8_t g, uint8_t b)
{
    s2h_cmd(S2H_TINTSET);
    s2h_byte(r); s2h_byte(g); s2h_byte(b);
    s2h_end();
}

static void s2h_init(void)
{
    // clocks: GPIOC + SPI1
    RCC->APB2PCENR |= RCC_APB2Periph_GPIOC | RCC_APB2Periph_SPI1;

    // PC3 (DC), PC4 (CS): push-pull outputs
    GPIOC->CFGLR &= ~((0xf << (4*3)) | (0xf << (4*4)));
    GPIOC->CFGLR |=  (GPIO_Speed_10MHz | GPIO_CNF_OUT_PP) << (4*3);
    GPIOC->CFGLR |=  (GPIO_Speed_10MHz | GPIO_CNF_OUT_PP) << (4*4);
    S2H_CS_HIGH();
    S2H_DC_DAT();

    // PC5 (SCK), PC6 (MOSI): alternate function push-pull, fast
    GPIOC->CFGLR &= ~((0xf << (4*5)) | (0xf << (4*6)));
    GPIOC->CFGLR |=  (GPIO_Speed_50MHz | GPIO_CNF_OUT_PP_AF) << (4*5);
    GPIOC->CFGLR |=  (GPIO_Speed_50MHz | GPIO_CNF_OUT_PP_AF) << (4*6);

    // SPI1: master, mode 0, 8 bit, MSB first, HCLK/2 = 24MHz
    SPI1->CTLR1 = SPI_NSS_Soft | SPI_CPHA_1Edge | SPI_CPOL_Low |
                  SPI_DataSize_8b | SPI_Mode_Master |
                  SPI_Direction_1Line_Tx | SPI_BaudRatePrescaler_2;
    SPI1->CTLR1 |= CTLR1_SPE_Set;

    Delay_Ms(2);
    s2h_cmd(S2H_SWRESET);
    s2h_end();
    Delay_Ms(2);
}

#endif
