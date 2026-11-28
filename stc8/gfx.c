#include "gfx.h"
#include "assets.h"

static uint8_t wbytes = 20, rows = 120;

void gfx_cmd1(uint8_t c, uint8_t p) { hal_cmd(c); hal_data(p); hal_end(); }

void gfx_window(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1)
{
    hal_cmd(0x2A); hal_data(0); hal_data(x0); hal_data(0); hal_data(x1); hal_end();
    hal_cmd(0x2B); hal_data(0); hal_data(y0); hal_data(0); hal_data(y1); hal_end();
}

void gfx_clear(void)
{
    uint16_t n = (uint16_t)wbytes * rows;
    gfx_window(0, 0, wbytes * 8 - 1, rows - 1);
    hal_cmd(0x2C);
    while (n--) hal_data(0);
    hal_end();
}

void gfx_mode(uint8_t scale_code)
{
    gfx_cmd1(0x88, scale_code);
    if (scale_code == 3) { wbytes = 10; rows = 60; } else { wbytes = 20; rows = 120; }
    gfx_clear();
}

static uint8_t glyph(char c)
{
    static const char __code order[] = FONT_CHARS;
    uint8_t i;
    for (i = 0; order[i]; i++) if (order[i] == c) return i;
    return 0;
}

void gfx_text(uint8_t col, uint8_t y, const char *s)
{
    uint8_t n = 0, r, i;
    while (s[n]) n++;
    if (!n) return;
    gfx_window(col * 8, y, (col + n) * 8 - 1, y + 7);
    hal_cmd(0x2C);
    for (r = 0; r < 8; r++)
        for (i = 0; i < n; i++) hal_data(font8[(uint16_t)glyph(s[i]) * 8 + r]);
    hal_end();
}

void gfx_number(uint8_t col, uint8_t y, uint16_t v)
{
    char b[6];
    uint8_t i;
    for (i = 5; i--; ) { b[i] = '0' + v % 10; v /= 10; }
    b[5] = 0;
    gfx_text(col, y, b);
}
