/* PC stand-in for the board: decodes the byte stream sent to the module into a 1-bit picture, feeds
 * scripted key presses and dumps frames as PGM.
 *   host menu <ticks> <script> <dump ticks...>              run the real firmware main()
 *   host rom <file> <quirks> <rate> <ticks> <script> <dump ticks...>   run one CHIP-8 file directly
 * script: one character per 1/60 s tick; '.' = nothing, '_' = space, '^' = ESC, others sent as typed. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../gfx.h"
static uint8_t fb[120][160], inv;
static int cmd = -1, np, x0, x1 = 159, y0, y1 = 119, cx, cy, fmt, scale = 0, sent;
static unsigned long tick, bytes, max_bytes, last_bytes;
static uint8_t par[8];
static const char *script; static unsigned long nframes; static int dump[64], ndump;
void hal_init(void) {}
void hal_cmd(uint8_t c) { cmd = c; np = 0; bytes++; if (c == 0x2C) { cx = x0; cy = y0; } if (c == 0x21) inv = 1; if (c == 0x20) inv = 0; }
void hal_end(void) { cmd = -1; }
void hal_data(uint8_t b)
{
    bytes++;
    if (cmd == 0x2C) {
        int w = scale == 3 ? 80 : 160, h = scale == 3 ? 60 : 120;
        if (fmt != 0x11) { fprintf(stderr, "pixel data in format %02x\n", fmt); exit(2); }
        if ((x0 & 7) || ((x1 + 1) & 7) || x1 >= w || y1 >= h || x0 > x1 || y0 > y1) { fprintf(stderr, "bad window %d..%d x %d..%d at tick %lu\n", x0, x1, y0, y1, tick); exit(2); }
        if (cy > y1) { fprintf(stderr, "write past the window at tick %lu\n", tick); exit(2); }
        for (int i = 0; i < 8; i++) { fb[cy][cx] = (b >> (7 - i)) & 1; if (++cx > x1) { cx = x0; cy++; } }
        return;
    }
    if (np < 8) par[np++] = b;
    if (cmd == 0x2A && np == 4) { x0 = par[0] << 8 | par[1]; x1 = par[2] << 8 | par[3]; }
    if (cmd == 0x2B && np == 4) { y0 = par[0] << 8 | par[1]; y1 = par[2] << 8 | par[3]; }
    if (cmd == 0x3A && np == 1) fmt = par[0];
    if (cmd == 0x88 && np == 1) scale = par[0];
}
static void save(unsigned long t)
{
    char n[64]; int w = scale == 3 ? 80 : 160, h = scale == 3 ? 60 : 120;
    snprintf(n, sizeof n, "build/frame_%05lu.pgm", t);
    FILE *f = fopen(n, "wb"); fprintf(f, "P5 %d %d 255\n", w, h);
    for (int y = 0; y < h; y++) for (int x = 0; x < w; x++) fputc((fb[y][x] ^ inv) ? 255 : 0, f);
    fclose(f);
}
uint8_t hal_key(void)
{
    char c;
    if (sent || !script || tick >= strlen(script)) return 0;
    sent = 1; c = script[tick];
    return c == '.' ? 0 : c == '_' ? ' ' : c == '^' ? 27 : (uint8_t)c;
}
void hal_wait_tick(void)
{
    unsigned long d = bytes - last_bytes; last_bytes = bytes;
    if (d > max_bytes) max_bytes = d;
    for (int i = 0; i < ndump; i++) if ((unsigned long)dump[i] == tick) save(tick);
    sent = 0;
    if (++tick >= nframes) { printf("ticks %lu, bytes %lu, max bytes in one tick %lu\n", tick, bytes, max_bytes); exit(0); }
}
void game_main(void);
int main(int argc, char **argv)
{
    int a;
    if (!strcmp(argv[1], "menu")) {
        nframes = atol(argv[2]); script = argv[3];
        for (a = 4; a < argc && ndump < 64; a++) dump[ndump++] = atoi(argv[a]);
        game_main();
    } else {
        static uint8_t rom[4096];
        FILE *f = fopen(argv[2], "rb"); size_t n = fread(rom, 1, sizeof rom, f); fclose(f);
        nframes = atol(argv[5]); script = argv[6];
        for (a = 7; a < argc && ndump < 64; a++) dump[ndump++] = atoi(argv[a]);
        hal_cmd(0x3A); hal_data(0x11); hal_end();
        chip8_run("TEST", rom, (uint16_t)n, (uint8_t)atoi(argv[4]), (uint8_t)atoi(argv[3]));
        printf("chip8_run returned at tick %lu\n", tick);
    }
    return 0;
}
