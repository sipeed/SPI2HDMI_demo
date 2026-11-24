#pragma once
#include <stdint.h>
#define DELAY_MS_TIME 6000u
#define S2H_MODESET 0x8b
#define S2H_SCALESET 0x88
#define S2H_COLMOD 0x3a
#define S2H_RAMWR 0x2c
uint32_t funSysTick32(void);
void host_uart(char c);
uint8_t host_gpio(void);
void host_poll(void);
void host_serial(uint8_t c);
void s2h_cmd1(uint8_t c,uint8_t p);
void s2h_tint(uint8_t r,uint8_t g,uint8_t b);
void s2h_window(unsigned x0,unsigned y0,unsigned x1,unsigned y1);
void s2h_cmd(uint8_t c);
void s2h_byte(uint8_t b);
void s2h_end(void);
