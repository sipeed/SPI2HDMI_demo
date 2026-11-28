/*
 * SPI2HDMI arcade for the 8-pin STC8G1K17A: a menu, a native dino game and a CHIP-8 console.
 * Menu keys: W / S / A / D move, space or Enter starts.  ESC inside a game returns here.
 */
#include "gfx.h"
#include "roms.h"

#define ROWS 9

static void cursor(uint8_t sel, uint8_t on)
{
    gfx_text(sel < ROWS ? 0 : 10, 28 + (sel % ROWS) * 9, on ? ">" : " ");
}

static void menu_draw(uint8_t sel)
{
    uint8_t i;
    gfx_mode(2);                                   /* 160x120 canvas, 4x zoom */
    gfx_text(2, 2, "SPI2HDMI  ARCADE");
    gfx_text(1, 14, "8-PIN 8051  1K RAM");
    gfx_text(1, 28, "DINO RUN");
    for (i = 0; i < NROMS && i < 2 * ROWS - 1; i++)
        gfx_text(i + 1 < ROWS ? 1 : 11, 28 + ((i + 1) % ROWS) * 9, roms[i].name);
    gfx_text(1, 111, "WSAD MOVE  SPACE:GO");
    cursor(sel, 1);
}

void main(void)
{
    uint8_t i, k, sel = 0, n = NROMS + 1;
    hal_init();
    if (n > 2 * ROWS) n = 2 * ROWS;
    for (i = 0; i < 30; i++) hal_wait_tick();      /* let the module finish booting */
    hal_cmd(0x01); hal_end();                      /* SWRESET */
    hal_wait_tick();
    gfx_cmd1(0x3A, 0x11);                          /* 1 bit per pixel */
    hal_cmd(0x89); hal_data(0xB0); hal_data(0xFF); hal_data(0x90); hal_end();   /* phosphor green */
    menu_draw(sel);
    for (;;) {
        hal_wait_tick();
        k = hal_key();
        if (!k) continue;
        if (k >= 'A' && k <= 'Z') k += 32;
        i = sel;
        if (k == 'w' && sel % ROWS) i = sel - 1;
        else if (k == 's' && sel % ROWS < ROWS - 1 && sel + 1 < n) i = sel + 1;
        else if (k == 'd' && sel < ROWS && n > ROWS) i = sel + ROWS < n ? sel + ROWS : n - 1;
        else if (k == 'a' && sel >= ROWS) i = sel - ROWS;
        else if (k == ' ' || k == '\r' || k == '\n' || k == 'j') {
            if (sel == 0) dino_run();
            else chip8_run(roms[sel - 1].name, roms[sel - 1].data, roms[sel - 1].size, roms[sel - 1].rate, roms[sel - 1].quirks);
            menu_draw(sel);
            continue;
        }
        if (i != sel) { cursor(sel, 0); sel = i; cursor(sel, 1); }
    }
}
