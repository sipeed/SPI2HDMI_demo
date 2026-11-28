/* Drawing helpers shared by the menu and the games (1 bit per pixel canvas). */
#ifndef GFX_H
#define GFX_H
#include "hal.h"
void gfx_mode(uint8_t scale_code);                 /* 2 = 160x120, 3 = 80x60 (640x480 output); clears */
void gfx_cmd1(uint8_t c, uint8_t p);
void gfx_window(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1);
void gfx_clear(void);
void gfx_text(uint8_t col, uint8_t y, const char *s);   /* 8x8 cells; col in characters */
void gfx_number(uint8_t col, uint8_t y, uint16_t v);    /* 5 digits */
void dino_run(void);
void chip8_run(const char *name, const uint8_t __code *rom, uint16_t size, uint8_t rate, uint8_t quirks);
#endif
