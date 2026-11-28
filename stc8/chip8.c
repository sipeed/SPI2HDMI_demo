/*
 * CHIP-8 interpreter for a 1K-RAM 8051.
 *
 * The program stays in flash.  CHIP-8 memory writes (FX55, FX33) go to 32-byte pages that are
 * copied from flash on first write, so a 3.5K program runs with 512 bytes of writable memory.
 * The 64x32 picture is kept in RAM (sprites are XORed and collisions read back) and every sprite
 * draw sends only the bytes it touched to the module: 8x zoom of an 80x60 canvas, picture centred.
 *
 * Serial keys (hex keypad): 1 2 3 4 / Q W E R / A S D F / Z X C V  =  1 2 3 C / 4 5 6 D / 7 8 9 E / A 0 B F
 * ESC returns to the menu.  A key counts as held for KEY_HOLD ticks after each received character.
 */
#include "gfx.h"

#define PAGE     32
#define NPOOL    16
#define NMAP     112              /* pages covering 0x200..0xFFF */
#define OX       8                /* picture position on the 80x60 canvas (x byte aligned) */
#define OY       14
#define KEY_HOLD 10

#define Q_SHIFT 1                 /* 8XY6/8XYE shift VX in place */
#define Q_LOAD  2                 /* FX55/FX65 leave I unchanged */
#define Q_CLIP  4                 /* sprites clip at the edges instead of wrapping */
#define Q_VBL   8                 /* a sprite draw ends the frame */
#define Q_LOGIC 16                /* 8XY1/2/3 clear VF */

static __xdata uint8_t fb[256];
static __xdata uint8_t pool[NPOOL * PAGE];
static __xdata uint8_t map[NMAP];
static __xdata uint16_t stack[16];
static __xdata uint8_t V[16];
static __xdata uint8_t keyt[16];

static const uint8_t __code *rom;
static uint16_t romsz, I, pc, seed;
static uint8_t sp, dt, st, npool, quirks, stop, newkey, lastkey, frame_done;

static const uint8_t __code hexfont[80] = {
    0xF0,0x90,0x90,0x90,0xF0, 0x20,0x60,0x20,0x20,0x70, 0xF0,0x10,0xF0,0x80,0xF0, 0xF0,0x10,0xF0,0x10,0xF0,
    0x90,0x90,0xF0,0x10,0x10, 0xF0,0x80,0xF0,0x10,0xF0, 0xF0,0x80,0xF0,0x90,0xF0, 0xF0,0x10,0x20,0x40,0x40,
    0xF0,0x90,0xF0,0x90,0xF0, 0xF0,0x90,0xF0,0x10,0xF0, 0xF0,0x90,0xF0,0x90,0x90, 0xE0,0x90,0xE0,0x90,0xE0,
    0xF0,0x80,0x80,0x80,0xF0, 0xE0,0x90,0x90,0x90,0xE0, 0xF0,0x80,0xF0,0x80,0xF0, 0xF0,0x80,0xF0,0x80,0x80,
};
static const char __code keymap[16] = { 'x','1','2','3','q','w','e','a','s','d','z','c','4','r','f','v' };

static uint8_t rd(uint16_t a)
{
    uint8_t m;
    a &= 0xFFF;
    if (a < 0x200) return a < 80 ? hexfont[a] : 0;
    a -= 0x200;
    m = map[a >> 5];
    if (m != 0xFF) return pool[(uint16_t)m * PAGE + (a & 31)];
    return a < romsz ? rom[a] : 0;
}

static void wr(uint16_t a, uint8_t v)
{
    uint8_t m, i, pg;
    uint16_t base;
    a &= 0xFFF;
    if (a < 0x200) return;
    a -= 0x200;
    pg = (uint8_t)(a >> 5);
    m = map[pg];
    if (m == 0xFF) {
        if (npool == NPOOL) { stop = 2; return; }          /* out of writable memory */
        m = npool++;
        map[pg] = m;
        base = (uint16_t)pg * PAGE;
        for (i = 0; i < PAGE; i++) pool[(uint16_t)m * PAGE + i] = (base + i) < romsz ? rom[base + i] : 0;
    }
    pool[(uint16_t)m * PAGE + (a & 31)] = v;
}

static void flush(uint8_t bx0, uint8_t bx1, uint8_t y0, uint8_t y1)
{
    uint8_t x, y;
    gfx_window(OX + bx0 * 8, OY + y0, OX + bx1 * 8 + 7, OY + y1);
    hal_cmd(0x2C);
    for (y = y0; y <= y1; y++)
        for (x = bx0; x <= bx1; x++) hal_data(fb[(uint16_t)y * 8 + x]);
    hal_end();
}

static void draw(uint8_t x, uint8_t y, uint8_t n)
{
    uint8_t r, yy, bits, a, b, s, bx, bx1, wrapped = 0, rows = 0;
    uint16_t p;
    x &= 63; y &= 31;
    s = x & 7; bx = x >> 3; bx1 = bx + 1;
    V[15] = 0;
    for (r = 0; r < n; r++) {
        yy = y + r;
        if (yy > 31) { if (quirks & Q_CLIP) break; yy &= 31; wrapped = 1; }
        bits = rd(I + r);
        a = bits >> s;
        b = s ? (uint8_t)(bits << (8 - s)) : 0;
        p = (uint16_t)yy * 8 + bx;
        if (fb[p] & a) V[15] = 1;
        fb[p] ^= a;
        if (b) {
            if (bx1 > 7) { if (quirks & Q_CLIP) { rows++; continue; } p -= 7; wrapped = 1; } else p++;
            if (fb[p] & b) V[15] = 1;
            fb[p] ^= b;
        }
        rows++;
    }
    if (wrapped) flush(0, 7, 0, 31);
    else if (rows) flush(bx, (s && bx1 < 8) ? bx1 : bx, y, y + rows - 1);
    if (quirks & Q_VBL) frame_done = 1;
}

static void poll(void)
{
    uint8_t c, i;
    while ((c = hal_key()) != 0) {
        if (c == KEY_ESC) { stop = 1; return; }
        if (c >= 'A' && c <= 'Z') c += 32;
        for (i = 0; i < 16; i++) if (keymap[i] == c) { keyt[i] = KEY_HOLD; lastkey = i; newkey = 1; }
    }
}

static void tick(void)
{
    uint8_t i;
    hal_wait_tick();
    seed += 0x9E37;
    if (dt) dt--;
    if (st) st--;
    for (i = 0; i < 16; i++) if (keyt[i]) keyt[i]--;
    poll();
}

static void step(void)
{
    uint8_t hi = rd(pc), nn = rd(pc + 1), x = hi & 15, y = nn >> 4, vx, vy, t;
    uint16_t nnn = ((uint16_t)x << 8) | nn;
    pc += 2;
    vx = V[x]; vy = V[y];
    switch (hi >> 4) {
    case 0x0:
        if (nn == 0xE0) { for (nnn = 0; nnn < 256; nnn++) fb[nnn] = 0; flush(0, 7, 0, 31); }
        else if (nn == 0xEE) { if (sp) pc = stack[--sp]; else stop = 3; }
        break;
    case 0x1: pc = nnn; break;
    case 0x2: if (sp < 16) { stack[sp++] = pc; pc = nnn; } else stop = 3; break;
    case 0x3: if (vx == nn) pc += 2; break;
    case 0x4: if (vx != nn) pc += 2; break;
    case 0x5: if (vx == vy) pc += 2; break;
    case 0x6: V[x] = nn; break;
    case 0x7: V[x] = vx + nn; break;
    case 0x8:
        switch (nn & 15) {
        case 0x0: V[x] = vy; break;
        case 0x1: V[x] = vx | vy; if (quirks & Q_LOGIC) V[15] = 0; break;
        case 0x2: V[x] = vx & vy; if (quirks & Q_LOGIC) V[15] = 0; break;
        case 0x3: V[x] = vx ^ vy; if (quirks & Q_LOGIC) V[15] = 0; break;
        case 0x4: t = vx + vy; V[x] = t; V[15] = t < vx; break;
        case 0x5: V[x] = vx - vy; V[15] = vx >= vy; break;
        case 0x6: if (quirks & Q_SHIFT) vy = vx; V[x] = vy >> 1; V[15] = vy & 1; break;
        case 0x7: V[x] = vy - vx; V[15] = vy >= vx; break;
        case 0xE: if (quirks & Q_SHIFT) vy = vx; V[x] = vy << 1; V[15] = vy >> 7; break;
        }
        break;
    case 0x9: if (vx != vy) pc += 2; break;
    case 0xA: I = nnn; break;
    case 0xB: pc = nnn + V[0]; break;
    case 0xC:
        seed = (seed >> 1) ^ ((seed & 1) ? 0xB400 : 0);
        V[x] = (uint8_t)(seed ^ (seed >> 8)) & nn;
        break;
    case 0xD: draw(vx, vy, nn & 15); break;
    case 0xE:
        t = keyt[vx & 15] != 0;
        if ((nn == 0x9E && t) || (nn == 0xA1 && !t)) pc += 2;
        break;
    case 0xF:
        switch (nn) {
        case 0x07: V[x] = dt; break;
        case 0x0A: newkey = 0; while (!newkey && !stop) tick(); V[x] = lastkey; frame_done = 1; break;
        case 0x15: dt = vx; break;
        case 0x18: st = vx; break;
        case 0x1E: I += vx; break;
        case 0x29: I = (uint16_t)(vx & 15) * 5; break;
        case 0x33: wr(I, vx / 100); wr(I + 1, vx / 10 % 10); wr(I + 2, vx % 10); break;
        case 0x55: for (t = 0; t <= x; t++) wr(I + t, V[t]); if (!(quirks & Q_LOAD)) I += x + 1; break;
        case 0x65: for (t = 0; t <= x; t++) V[t] = rd(I + t); if (!(quirks & Q_LOAD)) I += x + 1; break;
        }
        break;
    }
}

void chip8_run(const char *name, const uint8_t __code *r, uint16_t size, uint8_t rate, uint8_t q)
{
    uint16_t i;
    uint8_t n;
    rom = r; romsz = size; quirks = q;
    for (i = 0; i < 256; i++) fb[i] = 0;
    for (n = 0; n < NMAP; n++) map[n] = 0xFF;
    for (n = 0; n < 16; n++) { V[n] = 0; keyt[n] = 0; }
    pc = 0x200; I = 0; sp = 0; dt = 0; st = 0; npool = 0; stop = 0; newkey = 0;
    if (!seed) seed = 0xACE1;

    gfx_mode(3);                                       /* 80x60 canvas, 8x zoom */
    gfx_text(1, 2, name);
    gfx_window(0, OY - 2, 79, OY - 2); hal_cmd(0x2C); for (n = 0; n < 10; n++) hal_data(0xFF); hal_end();
    gfx_window(0, OY + 33, 79, OY + 33); hal_cmd(0x2C); for (n = 0; n < 10; n++) hal_data(0xFF); hal_end();
    gfx_text(1, 50, "ESC:MENU");

    while (!stop) {
        tick();
        frame_done = 0;
        for (n = rate; n && !stop && !frame_done; n--) step();
    }
    if (stop > 1) {                                    /* fault: show why for a second */
        gfx_text(1, 50, stop == 2 ? "NO RAM  " : "STACK   ");
        for (n = 0; n < 60; n++) hal_wait_tick();
    }
}
