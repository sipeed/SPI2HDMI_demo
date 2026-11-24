/*
 * spi2hdmi.h - portable single-header MCU driver for the Sipeed SPI2HDMI module
 * (classic 4-wire SPI mode).
 *
 * The module behaves like an ILI9341 4-line SPI TFT (CS/SCK/MOSI/DC,
 * SPI mode 0, MSB first, SCK up to 80MHz) with a 640x480@60 (default) or
 * 1280x720@30 HDMI output.
 * Existing ILI9341 code (CASET/PASET/RAMWR flow) works as-is; the extension
 * commands add zoom, extra color depths, gray tinting and a 16-color palette.
 *
 * Porting: implement the three callbacks in spi2hdmi_hal_t.  CS handling may
 * be done in spi_write itself (transaction per call) or via the cs callback.
 */
#ifndef SPI2HDMI_H
#define SPI2HDMI_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---- commands (ILI9341 compatible subset) ---- */
#define S2H_SWRESET  0x01
#define S2H_INVOFF   0x20
#define S2H_INVON    0x21
#define S2H_DISPOFF  0x28
#define S2H_DISPON   0x29
#define S2H_CASET    0x2A
#define S2H_PASET    0x2B
#define S2H_RAMWR    0x2C
#define S2H_RAMWRC   0x3C
#define S2H_MADCTL   0x36
#define S2H_COLMOD   0x3A
/* ---- extension commands ---- */
#define S2H_SCALESET 0x88   /* 1 param: see s2h_scale_t */
#define S2H_TINTSET  0x89   /* 3 params: R,G,B full-scale color of gray modes */
#define S2H_PALSET   0x8A   /* 16 params: RGB332 palette entries 0..15 */
#define S2H_MODESET  0x8B   /* 1 param: see s2h_vmode_t (video re-locks ~ms) */

/* COLMOD parameter values */
typedef enum {
    S2H_FMT_RGB565 = 0x55,  /* 2 bytes/px, hi byte first (ILI9341 standard) */
    S2H_FMT_RGB332 = 0x32,  /* 1 byte/px */
    S2H_FMT_GRAY4  = 0x14,  /* 2 px/byte, MSB first; x0 must be even   */
    S2H_FMT_PAL16  = 0x24,  /* 2 px/byte, 16-color palette; x0 even    */
    S2H_FMT_GRAY2  = 0x12,  /* 4 px/byte, MSB first; x0 multiple of 4  */
    S2H_FMT_MONO1  = 0x11,  /* 8 px/byte, MSB first; x0 multiple of 8  */
} s2h_fmt_t;

/* MODESET parameter: output video mode */
typedef enum {
    S2H_MODE_640x480_60  = 0,  /* default */
    S2H_MODE_1280x720_30 = 1,  /* some PC monitors reject 30Hz; TVs are fine */
} s2h_vmode_t;

/* SCALESET parameter: pixel-replication zoom, counted from the mode's base.
 *   640x480 mode  codes 0..4: 640x480 / 320x240 / 160x120 / 80x60 / 40x30
 *   1280x720 mode codes 0..3: 640x360 / 320x180 / 160x90 / 80x45
 * (720p always replicates at least 2x, so its code 0 is already 640x360.)
 * MODESET resets the scale code to 0 and the window to the full screen. */
typedef enum {
    S2H_SCALE_1X  = 0,   /* 640x480 mode */
    S2H_SCALE_2X  = 1,
    S2H_SCALE_4X  = 2,
    S2H_SCALE_8X  = 3,
    S2H_SCALE_16X = 4,
    S2H_SCALE_720P_2X  = 0,   /* 1280x720 mode */
    S2H_SCALE_720P_4X  = 1,
    S2H_SCALE_720P_8X  = 2,
    S2H_SCALE_720P_16X = 3,
} s2h_scale_t;

typedef struct {
    /* send n bytes over SPI (CS asserted for the whole call) */
    void (*spi_write)(const uint8_t *data, size_t n);
    /* drive the D/C line: 0 = command, 1 = data */
    void (*dc)(int level);
    /* assert (1) / deassert (0) chip select, active low on the wire;
     * pass NULL if spi_write manages CS itself */
    void (*cs)(int select);
} spi2hdmi_hal_t;

typedef struct {
    spi2hdmi_hal_t hal;
    uint16_t width;   /* current logical width  */
    uint16_t height;  /* current logical height */
    uint8_t  mode;    /* current video mode     */
} spi2hdmi_t;

static inline void s2h__cs(spi2hdmi_t *d, int s)
{
    if (d->hal.cs) d->hal.cs(s);
}

static inline void s2h_cmd(spi2hdmi_t *d, uint8_t c,
                           const uint8_t *param, size_t n)
{
    s2h__cs(d, 1);
    d->hal.dc(0);
    d->hal.spi_write(&c, 1);
    if (n) {
        d->hal.dc(1);
        d->hal.spi_write(param, n);
    }
    s2h__cs(d, 0);
}

static inline void s2h_init(spi2hdmi_t *d, const spi2hdmi_hal_t *hal)
{
    d->hal = *hal;
    d->width = 640;
    d->height = 480;
    d->mode = 0;
    s2h_cmd(d, S2H_SWRESET, NULL, 0);
    /* no sleep-out / power sequence needed, display is on after reset */
}

static inline void s2h_set_scale(spi2hdmi_t *d, s2h_scale_t s)
{
    uint8_t p = (uint8_t)s;
    s2h_cmd(d, S2H_SCALESET, &p, 1);
    d->width  = (uint16_t)(640 >> s);
    d->height = (uint16_t)((d->mode ? 360 : 480) >> s);
}

static inline void s2h_set_mode(spi2hdmi_t *d, s2h_vmode_t m)
{
    uint8_t p = (uint8_t)m;
    s2h_cmd(d, S2H_MODESET, &p, 1);
    d->mode = (uint8_t)m;
    d->width  = 640;              /* MODESET selects the mode's base zoom */
    d->height = m ? 360 : 480;
}

static inline void s2h_set_format(spi2hdmi_t *d, s2h_fmt_t f)
{
    uint8_t p = (uint8_t)f;
    s2h_cmd(d, S2H_COLMOD, &p, 1);
}

/* full-scale color of the grayscale modes (default white) */
static inline void s2h_set_tint(spi2hdmi_t *d, uint8_t r, uint8_t g, uint8_t b)
{
    uint8_t p[3] = { r, g, b };
    s2h_cmd(d, S2H_TINTSET, p, 3);
}

/* 16 RGB332 entries for S2H_FMT_PAL16 (SWRESET restores the CGA palette) */
static inline void s2h_set_palette(spi2hdmi_t *d, const uint8_t pal[16])
{
    s2h_cmd(d, S2H_PALSET, pal, 16);
}

static inline void s2h_set_window(spi2hdmi_t *d, uint16_t x0, uint16_t y0,
                                  uint16_t x1, uint16_t y1)
{
    uint8_t pc[4] = { (uint8_t)(x0 >> 8), (uint8_t)x0,
                      (uint8_t)(x1 >> 8), (uint8_t)x1 };
    uint8_t pr[4] = { (uint8_t)(y0 >> 8), (uint8_t)y0,
                      (uint8_t)(y1 >> 8), (uint8_t)y1 };
    s2h_cmd(d, S2H_CASET, pc, 4);
    s2h_cmd(d, S2H_PASET, pr, 4);
}

/* start RAMWR and stream raw pixel bytes (packed per current COLMOD) */
static inline void s2h_write_pixels(spi2hdmi_t *d,
                                    const uint8_t *data, size_t n)
{
    s2h_cmd(d, S2H_RAMWR, data, n);
}

/* fill the current window with one RGB565 color (RGB565 mode only) */
static inline void s2h_fill565(spi2hdmi_t *d, uint16_t x0, uint16_t y0,
                               uint16_t x1, uint16_t y1, uint16_t color)
{
    uint32_t n = (uint32_t)(x1 - x0 + 1) * (uint32_t)(y1 - y0 + 1);
    uint8_t px[2] = { (uint8_t)(color >> 8), (uint8_t)color };
    s2h_set_window(d, x0, y0, x1, y1);
    s2h__cs(d, 1);
    d->hal.dc(0);
    { uint8_t c = S2H_RAMWR; d->hal.spi_write(&c, 1); }
    d->hal.dc(1);
    while (n--)
        d->hal.spi_write(px, 2);
    s2h__cs(d, 0);
}

#ifdef __cplusplus
}
#endif
#endif /* SPI2HDMI_H */
