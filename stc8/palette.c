/*
 * Palette animation for the 8-pin STC8G1K17A on the Sipeed SPI2HDMI module.
 *
 * 640x480, 4 bits per pixel.  A scene is painted once, top to bottom; that is the only time pixel
 * data is sent (153,600 bytes).  After that the picture moves because the 16-entry palette is
 * rewritten sixty times a second: 17 bytes per frame.  The module keeps the picture and applies
 * the palette while it displays, so the MCU needs no frame buffer.
 *
 * Palette entry 0 stays black and 15 white (caption); 1..14 are taken from a cyclic gradient at a
 * moving phase.  Neighbouring entries are blended with a 4x4 ordered dither while painting.
 */
#include "hal.h"
#include "assets.h"
#include "paltab.h"

#define W 640
#define H 480
#define HOLD_TICKS 450                       /* 7.5 s of pure palette animation per scene */

static uint8_t scene;
static uint16_t phase, speed;
static __idata uint8_t dth[4];               /* dither offsets of the current row */

/* value 0..255 (cyclic) + dither -> palette index 1..14 */
#define QUANT(v, d) ((uint8_t)(((uint16_t)(uint8_t)((v) + (d)) * 14) >> 8) + 1)

static const uint8_t __code bayer[16] = { 0, 9, 2, 11,  14, 5, 16, 7,  3, 12, 1, 10,  17, 8, 15, 6 };

static void window(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1)
{
    hal_cmd(0x2A); hal_data(x0 >> 8); hal_data((uint8_t)x0); hal_data(x1 >> 8); hal_data((uint8_t)x1); hal_end();
    hal_cmd(0x2B); hal_data(y0 >> 8); hal_data((uint8_t)y0); hal_data(y1 >> 8); hal_data((uint8_t)y1); hal_end();
}

static void palette_step(void)
{
    const uint8_t __code *g = scene == 0 ? grad_hue : scene == 1 ? grad_ice : grad_fire;
    uint8_t i, p = phase >> 8;
    hal_cmd(0x8A);
    hal_data(0x00);
    for (i = 0; i < 14; i++, p += 18) hal_data(g[p]);
    hal_data(0xFF);
    hal_end();
    phase -= speed;
    hal_key();                               /* keeps the "@STCISP#" download trigger alive */
}

/* The wave value is computed once per two pixels (one output byte); the dither is still per pixel. */
#define EMIT2(v, k) hal_data((uint8_t)(QUANT(v, dth[k]) << 4) | QUANT(v, dth[(k) + 1]))

/* ---- scene 0: plasma, four sine waves ---- */
static void row_plasma(uint16_t y)
{
    uint16_t p1 = 0, p3 = y * 0x80u, p4 = 0u - y * 0x99u;
    uint8_t yt = sin8[(uint8_t)(y + 40)], n, v;
    for (n = 160; n; n--) {
        v = (uint8_t)(((uint16_t)sin8[p1 >> 8] + yt + sin8[p3 >> 8] + sin8[p4 >> 8]) >> 1);
        p1 += 0x180; p3 += 0x100; p4 += 0xCC;
        EMIT2(v, 0);
        v = (uint8_t)(((uint16_t)sin8[p1 >> 8] + yt + sin8[p3 >> 8] + sin8[p4 >> 8]) >> 1);
        p1 += 0x180; p3 += 0x100; p4 += 0xCC;
        EMIT2(v, 2);
    }
}

/* ---- scene 1: two circular waves interfering.  The distance to each centre is tracked
 *      incrementally (r, and e = distance^2 - r^2), so there is no multiply or square root. ---- */
typedef struct { uint16_t r; int16_t e, d; } ring_t;
static ring_t ra, rb;                        /* state at x = 0 of the current row */
static int16_t rdy_a, rdy_b;

static void ring_init(ring_t *g, int16_t cx, int16_t cy)
{
    uint32_t d2 = (uint32_t)((int32_t)cx * cx) + (uint32_t)((int32_t)cy * cy);
    uint16_t r = 0;
    while ((uint32_t)(r + 1) * (r + 1) <= d2) r++;
    g->r = r; g->e = (int16_t)(d2 - (uint32_t)r * r); g->d = -cx;
}
#define RING_FIX(r, e) \
    while ((e) > (int16_t)((r) << 1)) { (e) -= ((r) << 1) + 1; (r)++; } \
    while ((e) < 0) { (r)--; (e) += ((r) << 1) + 1; }

static void row_rings(void)
{
    uint16_t r1 = ra.r, r2 = rb.r;
    int16_t e1 = ra.e, e2 = rb.e, d1 = ra.d, d2 = rb.d;
    uint8_t n, v;
    for (n = 160; n; n--) {                                       /* one wave value per four pixels */
        v = (uint8_t)(((uint16_t)sin8[(uint8_t)(r1 * 3)] + sin8[(uint8_t)(r2 * 3)]) >> 1);
        e1 += (d1 << 3) + 16; d1 += 4; RING_FIX(r1, e1);
        e2 += (d2 << 3) + 16; d2 += 4; RING_FIX(r2, e2);
        EMIT2(v, 0); EMIT2(v, 2);
    }
    ra.e += (rdy_a << 1) + 1; rdy_a++; RING_FIX(ra.r, ra.e);      /* step both centres to the next row */
    rb.e += (rdy_b << 1) + 1; rdy_b++; RING_FIX(rb.r, rb.e);
}

/* ---- scene 2: wavy bands with two lines of large text in the opposite phase ---- */
static const char __code big1[] = "SIPEED";
static const char __code big2[] = "SPI2HDMI";

static uint8_t glyph_row(char c, uint8_t r)
{
    static const char __code order[] = FONT_CHARS;
    uint8_t i;
    for (i = 0; order[i]; i++) if (order[i] == c) break;
    if (!order[i]) i = 0;
    return (uint8_t)(font8[(uint16_t)i * 8 + r] << 1);            /* bit 7 = leftmost of 6 columns */
}

static void row_bands(uint16_t y)
{
    const char __code *t = 0;
    uint16_t p = 0, x, tx = 0;                                    /* x and tx count pixel pairs */
    uint8_t yt = (uint8_t)(y + (y >> 1)), sc = 0, gr = 0, v, bits = 0, cnt = 0, col = 6, on = 0;
    if (y >= 110 && y < 110 + 7 * 16)      { t = big1; sc = 8; tx = 20; gr = (uint8_t)((y - 110) >> 4); }
    else if (y >= 270 && y < 270 + 7 * 12) { t = big2; sc = 6; tx = 19; gr = (uint8_t)((y - 270) / 12); }
    for (x = 0; x < W / 2; x++) {
        v = yt + (uint8_t)(((uint16_t)sin8[p >> 8] * 85) >> 8);
        p += 0x154;
        if (t && x >= tx) {
            if (!cnt) {
                cnt = sc;
                if (col == 6) { col = 0; bits = *t ? glyph_row(*t++, gr) : 0; if (!bits && !*t) t = 0; }
                on = bits & 0x80; bits <<= 1; col++;
            }
            cnt--;
            if (on) v += 128;
        }
        if ((uint8_t)x & 1) EMIT2(v, 2); else EMIT2(v, 0);
    }
}

static void paint(void)
{
    uint16_t y;
    if (scene == 1) {
        ring_init(&ra, 200, 170); rdy_a = -170;
        ring_init(&rb, 450, 320); rdy_b = -320;
    }
    window(0, 0, W - 1, H - 1);
    hal_cmd(0x2C);
    for (y = 0; y < H; y++) {
        const uint8_t __code *d = bayer + (((uint8_t)y & 3) << 2);
        dth[0] = d[0]; dth[1] = d[1]; dth[2] = d[2]; dth[3] = d[3];
        if (scene == 0) row_plasma(y); else if (scene == 1) row_rings(); else row_bands(y);
        if (hal_tick_poll()) {               /* keep the colours moving while painting */
            hal_end();
            palette_step();
            hal_cmd(0x3C);                   /* continue the pixel stream where it stopped */
        }
    }
    hal_end();
}

/* black box with white 12x16 text near the bottom edge */
static void caption(void)
{
    static const char __code txt[] = "STC8G1K17A  8-PIN 8051  1K RAM  640X480";      /* 39 characters */
    uint8_t r, i, k, bits;
    window(80, 446, 80 + 480 - 1, 446 + 26 - 1);
    hal_cmd(0x2C);
    for (r = 0; r < 26; r++) {
        for (i = 0; i < 6; i++) hal_data(0x00);
        for (i = 0; i < 39; i++) {
            bits = (r >= 5 && r < 21) ? glyph_row(txt[i], (uint8_t)(r - 5) >> 1) : 0;
            for (k = 0; k < 6; k++, bits <<= 1) hal_data(bits & 0x80 ? 0xFF : 0x00);
        }
    }
    hal_end();
}

#ifdef TIMING            /* cycle-count build: paint one scene, then stop at timing_done() */
void timing_done(void) { for (;;) ; }
void main(void) { scene = TIMING; paint(); timing_done(); }
#else
void main(void)
{
    static const uint16_t __code speeds[3] = { 0x00DA, 0x01EB, 0x0148 };
    uint16_t t;
    hal_init();
    for (t = 0; t < 30; t++) hal_wait_tick();             /* let the module finish booting */
    hal_cmd(0x01); hal_end();                             /* SWRESET */
    hal_wait_tick();
    hal_cmd(0x3A); hal_data(0x24); hal_end();             /* 16-colour palette format */
    hal_cmd(0x88); hal_data(0); hal_end();                /* 1x: 640x480 canvas */
    for (;;) {
        for (scene = 0; scene < 3; scene++) {
            speed = speeds[scene];
            paint();
            caption();
            for (t = 0; t < HOLD_TICKS; t++) { hal_wait_tick(); palette_step(); }
        }
    }
}
#endif
