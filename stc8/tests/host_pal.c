/* PC stand-in for palette.c: decodes the stream sent to the module (16-colour format, palette
 * command, continued pixel writes) and writes one RGB frame per 1/60 s tick to stdout.
 *   host_pal <ticks> <pixel bytes the MCU manages per tick while painting, one value per scene...> | ffmpeg -f rawvideo -pix_fmt rgb24 -s 640x480 -r 60 -i - out.mp4 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../hal.h"
static uint8_t fb[480][640], pal[16], par[20];
static int cmd = -1, np, x0, x1 = 639, y0, y1 = 479, cx, cy, fmt;
static unsigned long tick, nticks, bytes, since, per_tick, pal_writes, max_since, rate[8];
static int nrate, paints;
static void frame(void)
{
    static uint8_t out[480 * 640 * 3];
    for (int i = 0; i < 480 * 640; i++) {
        uint8_t c = pal[((uint8_t *)fb)[i]];
        out[i * 3] = (c >> 5) * 255 / 7; out[i * 3 + 1] = ((c >> 2) & 7) * 255 / 7; out[i * 3 + 2] = (c & 3) * 255 / 3;
    }
    fwrite(out, 1, sizeof out, stdout);
    if (++tick >= nticks) { fprintf(stderr, "ticks %lu, bytes %lu, palette writes %lu\n", tick, bytes, pal_writes); exit(0); }
}
void hal_init(void) {}
void hal_cmd(uint8_t c)
{
    cmd = c; np = 0; bytes++;
    if (c == 0x2C) { cx = x0; cy = y0; if (x0 == 0 && x1 == 639 && y1 == 479) per_tick = rate[paints++ % nrate]; }
}
void hal_end(void) { if (cmd == 0x8A) { if (np != 16) { fprintf(stderr, "palette with %d bytes\n", np); exit(2); } memcpy(pal, par, 16); pal_writes++; } cmd = -1; }
void hal_data(uint8_t b)
{
    bytes++;
    if (cmd == 0x2C || cmd == 0x3C) {
        if (fmt != 0x24 || (x0 & 1) || !(x1 & 1) || x1 > 639 || y1 > 479) { fprintf(stderr, "bad pixel write: fmt %02x window %d..%d,%d..%d\n", fmt, x0, x1, y0, y1); exit(2); }
        if (cy > y1) { fprintf(stderr, "write past the window\n"); exit(2); }
        fb[cy][cx] = b >> 4; fb[cy][cx + 1] = b & 15; cx += 2; if (cx > x1) { cx = x0; cy++; }
        since++;
        return;
    }
    if (np < 20) par[np++] = b;
    if (cmd == 0x2A && np == 4) { x0 = par[0] << 8 | par[1]; x1 = par[2] << 8 | par[3]; }
    if (cmd == 0x2B && np == 4) { y0 = par[0] << 8 | par[1]; y1 = par[2] << 8 | par[3]; }
    if (cmd == 0x3A && np == 1) fmt = par[0];
}
uint8_t hal_key(void) { return 0; }
void hal_wait_tick(void) { since = 0; frame(); }
uint8_t hal_tick_poll(void) { if (since < per_tick) return 0; if (since > max_since) max_since = since; since = 0; frame(); return 1; }
void game_main(void);
int main(int argc, char **argv) { nticks = atol(argv[1]); for (int i = 2; i < argc && nrate < 8; i++) rate[nrate++] = atol(argv[i]); per_tick = rate[0]; for (int i = 0; i < 16; i++) pal[i] = 0; game_main(); return 0; }
