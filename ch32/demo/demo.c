/*
 * SPI2HDMI demo reel: Bad Apple clip -> Conway (3 classic structures) ->
 * fractals (Mandelbrot / Julia / XOR rainbow), looping forever.
 * Matches assets/demo_sequence.mp4 (which simulates this firmware's timing).
 *
 * CH32V003: 80x60 mono stages, 16KB flash clip
 * CH32V004: 160x120 mono stages, 32KB flash clip
 * Fractal stage is 320x240 RGB565 (2x zoom) on both parts - no framebuffer,
 * pixels stream over SPI as they are computed (Q11 fixed point, soft mul).
 */
#include "ch32fun.h"
#include "../common/s2h_ch32.h"
#include "fractal_tables.h"
#include "../common/packbits.h"

#if defined(CH32V004)
/* The test board has an active-high LED on PD6.  A 10 kHz SysTick interrupt
 * drives pulse-density modulation so it keeps breathing while the foreground
 * streams a frame over SPI. */
#define LED_PIN PD6
static volatile uint8_t led_level;

static void led_breathe_init(void)
{
    RCC->APB2PCENR |= RCC_APB2Periph_GPIOD;
    GPIOD->CFGLR &= ~(0xFu << (4 * 6));
    GPIOD->CFGLR |= (GPIO_Speed_10MHz | GPIO_CNF_OUT_PP) << (4 * 6);
    GPIOD->BSHR = 1u << (16 + 6);

    /* SysTick stays on HCLK/8, the same time base used by Delay_Ms. */
    SysTick->CTLR = 0;
    SysTick->CMP = DELAY_US_TIME * 100 - 1;
    SysTick->CNT = 0;
    SysTick->CTLR = SYSTICK_CTLR_STE | SYSTICK_CTLR_STIE;
    NVIC_EnableIRQ(SysTick_IRQn);
}

void SysTick_Handler(void) __attribute__((interrupt));
void SysTick_Handler(void)
{
    static uint8_t pdm_acc;
    static uint8_t envelope;
    static uint8_t envelope_div;
    static int8_t direction = 1;

    SysTick->CMP += DELAY_US_TIME * 100;
    SysTick->SR = 0;

    uint8_t previous = pdm_acc;
    pdm_acc += led_level;
    GPIOD->BSHR = (pdm_acc < previous) ? (1u << 6) : (1u << (16 + 6));

    /* One envelope step per 5 ms: about 1.28 s up and 1.28 s down.
     * Squaring the ramp better matches perceived LED brightness. */
    if (++envelope_div == 50) {
        envelope_div = 0;
        if (direction > 0) {
            if (++envelope == 255) direction = -1;
        } else if (envelope == 0) {
            direction = 1;
            envelope = 1;
        } else {
            --envelope;
        }
        led_level = ((uint16_t)envelope * envelope + 255) >> 8;
    }
}

#endif

#if defined(CH32V003)
#include "clip_80x60.h"
#define LIFE_W 80
#define LIFE_H 60
#define MONO_ZOOM 3
#else
#include "clip_160x120.h"
#define LIFE_W 160
#define LIFE_H 120
#define MONO_ZOOM 2
#endif

/* ================= shared stage memory =================
 * Bad Apple framebuffer and the Conway grid never coexist. */
#define FB_BYTES (CLIP_W * CLIP_H / 8)
#define WPR ((LIFE_W + 31) / 32)
static union {
    uint8_t  fb[FB_BYTES];
    uint32_t grid[2][LIFE_H][WPR];
} mem;
#define fb   mem.fb
#define grid mem.grid

static void mono_setup(uint16_t w, uint16_t h)
{
    s2h_cmd1(S2H_SCALESET, MONO_ZOOM);
    s2h_cmd1(S2H_COLMOD, S2H_FMT_MONO1);
    s2h_window(0, 0, w-1, h-1);
}

static void stage_badapple(void)
{
    mono_setup(CLIP_W, CLIP_H);
    s2h_tint(0xFF, 0xFF, 0xFF);
    for (uint16_t i = 0; i < FB_BYTES; i++) fb[i] = 0;
    const uint8_t *p = clip_data;
    const uint8_t *end = clip_data + sizeof(clip_data);
    for (uint16_t f = 0; f < CLIP_FRAMES; f++) {
        if (end - p < 2) return;
        uint16_t len = p[0] | (p[1] << 8);
        if (len > end - p - 2 ||
            !packbits_decode(p + 2, len, fb, FB_BYTES, !CLIP_NODELTA))
            return;
        p += 2 + len;
        s2h_cmd(S2H_RAMWR);
        for (uint16_t i = 0; i < FB_BYTES; i++) s2h_byte(fb[i]);
        s2h_end();
        Delay_Ms(1000 / CLIP_FPS - 4);
    }
}

/* ================= Conway ================= */
#define LASTMASK ((LIFE_W % 32) ? (0xFFFFFFFFu << (32 - (LIFE_W % 32))) : 0xFFFFFFFFu)
static uint8_t cur;

#define MAJ(a,b,c) (((a)&(b)) | ((c)&((a)^(b))))
static inline uint32_t shl_sp(const uint32_t *row, int w)
{
    uint32_t v = row[w] << 1;
    if (w + 1 < WPR) v |= row[w+1] >> 31;
    return v;
}
static inline uint32_t shr_sp(const uint32_t *row, int w)
{
    uint32_t v = row[w] >> 1;
    if (w > 0) v |= row[w-1] << 31;
    return v;
}

static void life_step(void)
{
    const uint32_t (*g)[WPR] = grid[cur];
    uint32_t (*n)[WPR] = grid[cur ^ 1];
    static const uint32_t zrow[WPR] = {0};
    for (int y = 0; y < LIFE_H; y++) {
        const uint32_t *ra = (y > 0) ? g[y-1] : zrow;
        const uint32_t *rb = g[y];
        const uint32_t *rc = (y < LIFE_H-1) ? g[y+1] : zrow;
        for (int w = 0; w < WPR; w++) {
            uint32_t a = ra[w], al = shr_sp(ra, w), ar = shl_sp(ra, w);
            uint32_t bl = shr_sp(rb, w), br = shl_sp(rb, w);
            uint32_t c = rc[w], cl = shr_sp(rc, w), cr = shl_sp(rc, w);
            uint32_t s0 = al^a^ar, c0 = MAJ(al,a,ar);
            uint32_t s1 = cl^c^cr, c1 = MAJ(cl,c,cr);
            uint32_t s2 = bl^br,   c2 = bl&br;
            uint32_t lo = s0^s1^s2, lc = MAJ(s0,s1,s2);
            uint32_t t0 = c0^c1^c2, t1 = MAJ(c0,c1,c2);
            uint32_t b1 = t0^lc, b1c = t0&lc;
            uint32_t b2 = t1^b1c, b3 = t1&b1c;
            uint32_t nx = ~b2 & ~b3 & b1 & (lo | rb[w]);
            n[y][w] = (w == WPR-1) ? (nx & LASTMASK) : nx;
        }
    }
    cur ^= 1;
}

static void life_send(void)
{
    s2h_cmd(S2H_RAMWR);
    for (int y = 0; y < LIFE_H; y++)
        for (int w = 0; w < WPR; w++) {
            uint32_t v = grid[cur][y][w];
            int nb = (w == WPR-1 && (LIFE_W % 32)) ? (LIFE_W % 32) / 8 : 4;
            for (int k = 0; k < nb; k++)
                s2h_byte((uint8_t)(v >> (24 - 8*k)));
        }
    s2h_end();
}

static void put_cell(int x, int y)
{
    if (x >= 0 && x < LIFE_W && y >= 0 && y < LIFE_H)
        grid[cur][y][x >> 5] |= 0x80000000u >> (x & 31);
}

/* patterns as (x,y) pairs */
static const uint8_t GUN[] = {0,4,0,5,1,4,1,5,10,4,10,5,10,6,11,3,11,7,12,2,
    12,8,13,2,13,8,14,5,15,3,15,7,16,4,16,5,16,6,17,5,20,2,20,3,20,4,21,2,
    21,3,21,4,22,1,22,5,24,0,24,1,24,5,24,6,34,2,34,3,35,2,35,3};
static const uint8_t RPENT[] = {1,0,2,0,0,1,1,1,1,2};
static const uint8_t LWSS[] = {0,0,3,0,4,1,0,2,4,2,1,3,2,3,3,3,4,3};

static void place(const uint8_t *cells, int n, int ox, int oy)
{
    for (int i = 0; i < n; i += 2)
        put_cell(ox + cells[i], oy + cells[i+1]);
}

static void clear_grid(void)
{
    for (int y = 0; y < LIFE_H; y++)
        for (int w = 0; w < WPR; w++)
            grid[cur][y][w] = 0;
}

static void stage_conway(void)
{
    mono_setup(LIFE_W, LIFE_H);
    for (int s = 0; s < 3; s++) {
        clear_grid();
        if (s == 0) {
            s2h_tint(0xFF, 0xFF, 0xFF);
            place(GUN, sizeof(GUN), LIFE_W/10, LIFE_H/8);
        } else if (s == 1) {
            s2h_tint(0x40, 0xFF, 0x60);
            place(RPENT, sizeof(RPENT), LIFE_W/2 - 2, LIFE_H/2 - 2);
        } else {
            s2h_tint(0x60, 0xB0, 0xFF);
            int step = LIFE_H / 5;
            for (int i = 0; i < 5; i++)
                place(LWSS, sizeof(LWSS), 4 + (i & 1) * 8, step/2 + i*step - 2);
        }
        for (int gen = 0; gen < 90; gen++) {
            life_send();
            life_step();
            Delay_Ms(16);
        }
    }
    s2h_tint(0xFF, 0xFF, 0xFF);
}

/* ================= fractals, 320x240 RGB565 streamed ================= */
#define FRAC_W 320
#define FRAC_H 240

static void px565(uint16_t p)
{
    s2h_byte(p >> 8);
    s2h_byte(p & 0xFF);
}

static void frac_setup(void)
{
    s2h_cmd1(S2H_SCALESET, 1);          /* 2x -> 320x240 logical */
    s2h_cmd1(S2H_COLMOD, S2H_FMT_RGB565);
    s2h_window(0, 0, FRAC_W-1, FRAC_H-1);
}

/* Q11 fixed point escape-time; julia!=0 uses (jcx,jcy) as constant */
static void escape_frac(int32_t x0, int32_t xstep, int32_t y0, int32_t ystep,
                        int julia, int32_t jcx, int32_t jcy,
                        const uint16_t *pal, const uint8_t *idxmap,
                        int npal, uint16_t inside)
{
    s2h_cmd(S2H_RAMWR);
    int32_t cy = y0;
    for (int py = 0; py < FRAC_H; py++, cy += ystep) {
        int32_t cx = x0;
        for (int px = 0; px < FRAC_W; px++, cx += xstep) {
            int32_t zx, zy, kx, ky;
            if (julia) { zx = cx >> 2; zy = cy >> 2; kx = jcx; ky = jcy; }
            else       { zx = 0; zy = 0; kx = cx >> 2; ky = cy >> 2; }
            int it;
            for (it = 0; it < 48; it++) {
                int32_t x2 = (zx * zx) >> 11;
                int32_t y2 = (zy * zy) >> 11;
                if (x2 + y2 > (4 << 11)) break;
                int32_t nzy = ((zx * zy) >> 10) + ky;
                zx = x2 - y2 + kx;
                zy = nzy;
            }
            if (it >= 48)
                px565(inside);
            else
                px565(pal[idxmap ? idxmap[it] : (it * npal) / 48]);
        }
    }
    s2h_end();
}

static void stage_fractals(void)
{
    frac_setup();

    /* Mandelbrot: x [-2.5,0.7], y [-1.2,1.2], coords accumulated in Q13 */
    escape_frac(-((int32_t)20480), 82, -((int32_t)9830), 82,
                0, 0, 0, mandel_pal, mandel_idx, 64, MANDEL_INSIDE);
    Delay_Ms(2500);

    /* Julia c = -0.8 + 0.156i: x [-1.8,1.8], y [-1.35,1.35] */
    escape_frac(-((int32_t)14746), 92, -((int32_t)11059), 92,
                1, (int32_t)(-0.8 * 2048), (int32_t)(0.156 * 2048),
                julia_pal, 0, 48, JULIA_INSIDE);
    Delay_Ms(2500);

    /* XOR rainbow sierpinski: brightness = x^y, hue = x+y (2x pixel coords
       to match the 640x480 appearance of the sim video) */
    frac_setup();
    s2h_cmd(S2H_RAMWR);
    for (int y = 0; y < FRAC_H; y++) {
        for (int x = 0; x < FRAC_W; x++) {
            uint32_t xx = x << 1, yy = y << 1;
            uint32_t v = (xx ^ yy) & 255;
            uint32_t h = ((xx + yy) >> 4) & 63;
            uint32_t r = (xor_r[h] * v) >> 8;
            uint32_t g = (xor_g[h] * v) >> 8;
            uint32_t b = (xor_b[h] * v) >> 8;
            px565(((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3));
        }
    }
    s2h_end();
    Delay_Ms(3000);
}

int main(void)
{
    SystemInit();
#if defined(CH32V004)
    led_breathe_init();
#endif
    s2h_init();
    while (1) {
        stage_badapple();
        Delay_Ms(300);
        stage_conway();
        Delay_Ms(300);
        stage_fractals();
        Delay_Ms(300);
    }
}
