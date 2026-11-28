/*
 * Dino run for an 8-pin 8051 (STC8G1K17A: 17K flash, 1K RAM) on the Sipeed SPI2HDMI module.
 *
 * The picture is 160x120, 1 bit per pixel, shown 4x enlarged on 640x480 HDMI.  The MCU keeps no
 * frame buffer: the module stores the picture, and every frame the 160x64 play field is composed
 * one 20-byte line at a time and streamed out (1280 bytes per frame).
 *
 * Serial keys: space / J / W = jump or start, ESC = back to the menu.
 */
#include "gfx.h"
#include "assets.h"

#define W_BYTES    20          /* 160 px */
#define BAND_Y     40          /* play field: canvas rows 40..103 */
#define BAND_H     64
#define GROUND_ROW 56          /* band row of the ground line; feet stand on the row above */
#define DINO_XB    2           /* dino at x = 16, byte aligned */
#define NOBS       2

static __idata uint8_t line[W_BYTES];
static __xdata int16_t obs_x[NOBS];    /* left edge in pixels; inactive when <= -16 */
static __xdata uint8_t obs_big[NOBS];
static uint8_t jump_i;         /* 0 = on the ground, else index into jump_y + 1 */
static uint8_t frame, speed, gap, rnd, hit, quit;
static __xdata uint16_t score, hiscore, ground_x;

/* height above the ground for each frame of a jump (peak 34 px) */
static const uint8_t __code jump_y[] = {
    5, 9, 13, 17, 20, 23, 26, 28, 30, 32, 33, 34, 34, 34, 33, 32, 30, 28, 26, 23, 20, 17, 13, 9, 5, 2
};
static const uint8_t __code pebbles[8] = { 0x00, 0x18, 0x00, 0x03, 0x40, 0x00, 0x0C, 0x80 };

static uint8_t rand8(void)
{
    rnd = (rnd >> 1) ^ ((rnd & 1) ? 0xB8 : 0);
    return rnd;
}

/* OR up to 16 pixels (hi:lo, MSB = leftmost) into the line at pixel x; flags a hit on overlap */
static void blit16(int16_t x, uint8_t hi, uint8_t lo)
{
    int8_t bi = (int8_t)(x >> 3);
    uint8_t s = (uint8_t)x & 7, b[3], i;
    uint16_t w = ((uint16_t)hi << 8 | lo);
    b[0] = (uint8_t)(w >> (8 + s));
    b[1] = (uint8_t)(w >> s);
    b[2] = s ? (uint8_t)(lo << (8 - s)) : 0;
    for (i = 0; i < 3; i++, bi++) {
        if (bi < 0 || bi >= W_BYTES) continue;
        if (line[bi] & b[i]) hit = 1;
        line[bi] |= b[i];
    }
}

/* compose and send the whole play field */
static void draw_band(uint8_t legs)
{
    uint8_t r, i, dr, top, h;
    const uint8_t __code *p;
    uint8_t dino_top = GROUND_ROW - DINO_H - (jump_i ? jump_y[jump_i - 1] : 0);
    uint8_t gs = (uint8_t)ground_x & 7, gb = (uint8_t)(ground_x >> 3);

    gfx_window(0, BAND_Y, 159, BAND_Y + BAND_H - 1);
    hal_cmd(0x2C);
    for (r = 0; r < BAND_H; r++) {
        for (i = 0; i < W_BYTES; i++) line[i] = 0;
        dr = r - dino_top;
        if (dr < DINO_H) {
            if (dr < DINO_BODY_ROWS) p = dino_body + (uint16_t)dr * 3;
            else p = (legs == 0 ? dino_legs0 : legs == 1 ? dino_legs1 : dino_legs2) + (uint16_t)(dr - DINO_BODY_ROWS) * 3;
            line[DINO_XB] = p[0]; line[DINO_XB + 1] = p[1]; line[DINO_XB + 2] = p[2];
        }
        for (i = 0; i < NOBS; i++) {
            if (obs_x[i] <= -16) continue;
            h = obs_big[i] ? CACT_B_H : CACT_S_H;
            top = GROUND_ROW - h;
            dr = r - top;
            if (dr >= h) continue;
            if (obs_big[i]) blit16(obs_x[i], cact_b[dr * 2], cact_b[dr * 2 + 1]);
            else            blit16(obs_x[i], cact_s[dr], 0);
        }
        if (r == GROUND_ROW) {
            for (i = 0; i < W_BYTES; i++) line[i] = 0xFF;
        } else if (r == GROUND_ROW + 3 || r == GROUND_ROW + 6) {
            for (i = 0; i < W_BYTES; i++) {
                uint8_t a = pebbles[(gb + i + r) & 7], b = pebbles[(gb + i + r + 1) & 7];
                line[i] = (uint8_t)(a << gs) | (gs ? (uint8_t)(b >> (8 - gs)) : 0);
            }
        }
        for (i = 0; i < W_BYTES; i++) hal_data(line[i]);
    }
    hal_end();
}

static uint8_t jump_key(void)
{
    uint8_t k = hal_key();
    if (k == KEY_ESC) quit = 1;
    return k == ' ' || k == 'j' || k == 'J' || k == 'w' || k == 'W';
}

static void reset_run(void)
{
    uint8_t i;
    for (i = 0; i < NOBS; i++) obs_x[i] = -16;
    jump_i = 0; score = 0; speed = 3; gap = 40; hit = 0; frame = 0;
}

static void hud(void)
{
    gfx_text(0, 8, "HI");   gfx_number(3, 8, hiscore);
    gfx_number(15, 8, score);
}

static void title(void)
{
    gfx_clear();
    gfx_text(3, 4, "SPI2HDMI  DINO");
    gfx_text(1, 18, "8-PIN 8051  1K RAM");
    gfx_text(4, 108, "PRESS  SPACE");
    reset_run();
    for (;;) {
        hal_wait_tick();
        frame++; ground_x += 2; rand8();
        draw_band(1 + ((frame >> 3) & 1));
        if (jump_key() || quit) break;
    }
    gfx_clear();
}

static void play(void)
{
    uint8_t i;
    reset_run();
    jump_i = 1;                                  /* the key that started the game also jumps */
    hud();
    for (;;) {
        hal_wait_tick();
        frame++;
        if (jump_key() && !jump_i) jump_i = 1;
        else if (jump_i && ++jump_i > sizeof jump_y) jump_i = 0;

        ground_x += speed;
        for (i = 0; i < NOBS; i++) if (obs_x[i] > -16) obs_x[i] -= speed;
        if (gap) gap--;
        else {
            for (i = 0; i < NOBS; i++) if (obs_x[i] <= -16) {
                obs_x[i] = 160; obs_big[i] = rand8() & 1;
                gap = 34 + (rand8() & 31) - speed * 2;
                break;
            }
        }
        hit = 0;
        draw_band(jump_i ? 0 : 1 + ((frame >> 2) & 1));
        if (hit || quit) break;
        if ((frame & 3) == 0) {
            score++;
            gfx_number(15, 8, score);
            if (score % 200 == 0 && speed < 6) speed++;
        }
    }
    if (quit) return;
    if (score > hiscore) hiscore = score;
    hud();
    gfx_text(5, 24, "GAME  OVER");
    for (i = 0; i < 6; i++) {                    /* flash the screen with the module's invert command */
        hal_cmd(i & 1 ? 0x20 : 0x21); hal_end();
        hal_wait_tick(); hal_wait_tick(); hal_wait_tick(); hal_wait_tick();
    }
    hal_cmd(0x20); hal_end();
    for (i = 0; i < 25; i++) { hal_wait_tick(); hal_key(); }   /* ignore keys for half a second */
    while (!jump_key() && !quit) { hal_wait_tick(); rand8(); }
    gfx_clear();
}

void dino_run(void)
{
    quit = 0;
    if (!rnd) rnd = 0x5A;
    title();
    while (!quit) play();
}
